#include "storage_engine.h"

StorageEngine::StorageEngine(const std::string &wal_path, size_t cache_size) : wal_(wal_path), memtable_(), seq_number_(1) {
    if (cache_size > 0) {
        cache_ = LRUCache(cache_size);
    }

    try {
        std::filesystem::create_directories("data/sstables");
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    std::ifstream metadataFile("data/metadata.txt");

    if (!metadataFile) {
        flush_counter_ = 0;
        levels_.resize(4);
    } else {
        std::string line;
        std::getline(metadataFile, line);
        flush_counter_ = stoull(line);

        std::getline(metadataFile, line);
        seq_number_ = stoull(line);

        loadLevelMetadata();
    }

    loadSSTables();

    // Start background compaction thread
    compaction_thread_ = std::thread(&StorageEngine::compactionThreadLoop, this);
}

StorageEngine::~StorageEngine() {
    // Signal shutdown and wake up compaction thread
    shutdown_.store(true, std::memory_order_release);
    compaction_cv_.notify_one();

    // Wait for thread to finish
    if (compaction_thread_.joinable()) {
        compaction_thread_.join();
    }
}

void StorageEngine::loadLevelMetadata() {
    std::ifstream levelFile("data/levels.txt");
    if (!levelFile) {
        levels_.resize(4);
        return;
    }

    levels_.clear();
    std::string line;

    while (std::getline(levelFile, line)) {
        if (line.empty())
            continue;

        std::istringstream iss(line);
        SSTableMeta meta;
        iss >> meta.id >> meta.level >> meta.minKey >> meta.maxKey >> meta.maxSeq >> meta.sizeBytes;

        if (meta.level >= levels_.size()) {
            levels_.resize(meta.level + 1);
        }

        levels_[meta.level].push_back(meta);
    }
}

void StorageEngine::loadSSTables() {
    sstables_.clear();

    for (const auto &levelMetas : levels_) {
        for (const auto &meta : levelMetas) {
            std::string path = "data/sstables/sstable_" + std::to_string(meta.id) + ".bin";
            if (std::filesystem::exists(path)) {
                sstables_.emplace_back(path);
            } else {
                std::cerr << "Warning: SSTable file was not found: " << path << '\n';
            }
        }
    }
}

bool StorageEngine::put(const std::string &key, const std::string &value) {
    bool result = false;
    if (memtable_.put(key, value, seq_number_)) {
        wal_.append(Operation::PUT, key, value, seq_number_);
        seq_number_++;
        result = true;

        if (cache_) {
            cache_->invalidate(key);
        }
    }
    checkFlush();
    return result;
}

bool StorageEngine::del(const std::string &key) {
    Entry existing;
    bool existed = get(key, existing);

    memtable_.del(key, seq_number_);
    wal_.append(Operation::DELETE, key, "", seq_number_);
    seq_number_++;

    if (cache_) {
        cache_->invalidate(key);
    }

    checkFlush();

    return existed;
}

bool StorageEngine::get(const std::string &key, Entry &out) const {
    if (cache_) {
        auto cached = cache_->get(key);
        if (cached) {
            out = *cached;
            return true;
        }
    }

    std::optional<Entry> candidate{};

    Entry mem;
    if (memtable_.get(key, mem)) {
        candidate = mem;
    }

    std::shared_lock<std::shared_mutex> lock(state_mutex_);

    // Search through the levels
    for (uint32_t level = 0; level < levels_.size(); level++) {
        if (levels_[level].empty())
            continue;

        if (level == 0) {
            // L0: Have to check all the files since there are overlapping ranges
            for (const auto &meta : levels_[0]) {
                auto it = std::find_if(sstables_.begin(), sstables_.end(), [&](const SSTable &sst) {
                    return sst.filename().find("_" + std::to_string(meta.id) + ".") != std::string::npos;
                });

                if (it != sstables_.end()) {
                    std::optional<Entry> record = it->get(key);
                    if (record && (!candidate || record->seq > candidate->seq)) {
                        candidate = *record;
                    }
                }
            }
        } else {
            // L1+: binary search to find correct file
            auto it = std::lower_bound(levels_[level].begin(), levels_[level].end(), key,
                                       [](const SSTableMeta &meta, const std::string &k) { return meta.maxKey < k; });

            if (it != levels_[level].end() && key >= it->minKey) {
                auto sstIt = std::find_if(sstables_.begin(), sstables_.end(), [&](const SSTable &sst) {
                    return sst.filename().find("_" + std::to_string(it->id) + ".") != std::string::npos;
                });

                if (sstIt != sstables_.end()) {
                    std::optional<Entry> record = sstIt->get(key);
                    if (record && (!candidate || record->seq > candidate->seq)) {
                        candidate = *record;
                    }
                    if (record) {
                        break;
                    }
                }
            }
        }
        if (candidate)
            break;
    }

    if (!candidate || candidate->type == EntryType::DELETE) {
        return false;
    }

    out = *candidate;

    if (cache_) {
        cache_->put(key, out);
    }

    return true;
}

void StorageEngine::ls() const {
    const auto currentMemtable = memtable_.snapshot();
    if (!currentMemtable.empty()) {
        std::cout << "Memtable:\n";
        for (const auto &[k, v] : currentMemtable) {
            std::cout << k << " " << (v.type == EntryType::DELETE ? "<TOMBSTONE>" : v.value) << " " << v.seq << "\n";
        }
        std::cout << '\n';
    }

    std::shared_lock<std::shared_mutex> lock(state_mutex_);

    for (size_t i = sstables_.size(); i-- > 0;) {
        const std::string path = sstables_[i].filename();
        if (!std::filesystem::exists(path)) {
            std::cerr << "Warning: SSTable file missing: " << path << ", skipping.\n";
            continue;
        }

        size_t underscorePos = path.rfind('_');
        size_t dotPos = path.rfind(".bin");
        if (underscorePos == std::string::npos || dotPos == std::string::npos || underscorePos >= dotPos)
            continue;

        std::string numberStr = path.substr(underscorePos + 1, dotPos - underscorePos - 1);
        std::cout << "SSTable " << numberStr << ":\n";

        std::map<std::string, Entry> currSStableData = sstables_[i].getData();
        for (const auto &[k, v] : currSStableData) {
            std::cout << k << " " << v.value << " " << v.seq << "\n";
        }
        std::cout << '\n';
    }
}

void StorageEngine::flush() {
    checkFlush(true);
}

void StorageEngine::recover() {
    uint64_t maxSeqNumber = seq_number_;

    if (!wal_.empty()) {
        wal_.replay([this, &maxSeqNumber](uint64_t seqNumber, Operation op, const std::string &key, const std::string &value) {
            maxSeqNumber = std::max(maxSeqNumber, seqNumber);
            switch (op) {
            case Operation::PUT:
                memtable_.put(key, value, seqNumber);
                break;
            case Operation::DELETE:
                memtable_.del(key, seqNumber);
                break;
            default:
                std::cerr << "Error reading operation\n";
            }
        });
        seq_number_ = maxSeqNumber + 1;
    }
}

void StorageEngine::handleCommand(const std::string &input) {
    std::string key;
    std::string value;
    Operation op = parseCommand(input, key, value);
    switch (op) {
    case Operation::PUT:
        put(key, value);
        break;
    case Operation::GET: {
        Entry result;
        get(key, result);
        std::cout << result.value << std::endl;
        break;
    }
    case Operation::LS: {
        ls();
        break;
    }
    case Operation::FLUSH: {
        flush();
        break;
    }
    case Operation::CLEAR: {
        clearData();
        break;
    }
    case Operation::DELETE:
        del(key);
        break;
    default:
        std::cerr << "Invalid command\n";
    }
}

void StorageEngine::checkFlush(bool debug) {
    static constexpr size_t kMemTableThreshold = 8 * 1024 * 1024;
    if (memtable_.getSize() >= kMemTableThreshold || debug) {
        wal_.flush();
        std::map<std::string, Entry> currentMemTable = memtable_.snapshot();
        const std::string dir_path = "data/sstables/";

        std::unique_lock<std::shared_mutex> lock(state_mutex_);

        flush_counter_++;

        SSTable newSSTable = SSTable::flush(currentMemTable, dir_path, flush_counter_);
        sstables_.push_back(std::move(newSSTable));

        SSTableMeta meta;
        meta.id = flush_counter_;
        meta.level = 0;
        meta.minKey = currentMemTable.begin()->first;
        meta.maxKey = currentMemTable.rbegin()->first;
        meta.maxSeq = seq_number_ - 1;
        meta.sizeBytes = std::filesystem::file_size(dir_path + "sstable_" + std::to_string(flush_counter_) + ".bin");

        if (levels_.empty())
            levels_.resize(4);

        levels_[0].push_back(meta);

        std::remove("data/log.bin");
        saveMetadata();

        memtable_.clear();

        if (cache_) {
            cache_->clear();
        }

        scheduleCompaction();
    }
}

void StorageEngine::clearData() {
    waitForCompaction();

    std::filesystem::path dataPath = "data";
    std::filesystem::create_directories(dataPath);

    try {
        if (std::filesystem::remove_all(dataPath) > 0) {
            memtable_.clear();
        } else {
            std::cerr << "The folder was not found or something went wrong" << std::endl;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    std::unique_lock<std::shared_mutex> lock(state_mutex_);

    memtable_.clear();
    sstables_.clear();
    flush_counter_ = 0;
    seq_number_ = 1;
    levels_.clear();
    levels_.resize(4);

    if (cache_) {
        cache_->clear();
    }

    try {
        std::filesystem::create_directories("data/sstables");
    } catch (const std::filesystem::filesystem_error &e) {
    }
}

void StorageEngine::scheduleCompaction() {
    compaction_needed_.store(true, std::memory_order_release);
    compaction_cv_.notify_one();
}

void StorageEngine::compactionThreadLoop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(compaction_mutex_);
            compaction_cv_.wait(lock, [this] {
                return shutdown_.load(std::memory_order_acquire) ||
                       (compaction_needed_.load(std::memory_order_acquire) && !compaction_paused_.load(std::memory_order_acquire));
            });
        }
        if (shutdown_.load(std::memory_order_acquire)) {
            break;
        }
        if (compaction_needed_.load(std::memory_order_acquire) && !compaction_paused_.load(std::memory_order_acquire)) {
            compaction_needed_.store(false, std::memory_order_release);
            compaction_in_progress_.store(true, std::memory_order_release);
            maybeCompactBackground();
            compaction_in_progress_.store(false, std::memory_order_release);
            // Notify anyone waiting for compaction to finish
            {
                std::lock_guard<std::mutex> lock(compaction_mutex_);
            }
            compaction_cv_.notify_all();
        }
    }
}

void StorageEngine::maybeCompactBackground() {
    // Check each level and compact if needed
    for (uint32_t level = 0; level < 3; level++) {
        bool needs_compaction = false;

        {
            std::shared_lock<std::shared_mutex> lock(state_mutex_);
            needs_compaction = shouldCompactUnlocked(level);
        }

        if (needs_compaction) {
            if (level == 0) {
                compactL0toL1();
            } else {
                compactlevelN(level);
            }
        }
    }
}

bool StorageEngine::shouldCompactUnlocked(uint32_t level) const {
    if (level >= levels_.size() || levels_[level].empty()) {
        return false;
    }

    if (level == 0) {
        return levels_[0].size() >= 4;
    }

    static const std::vector<uint64_t> klevelSizes = {0, 10 * 1024 * 1024, 100 * 1024 * 1024, 1024 * 1024 * 1024};

    if (level >= klevelSizes.size()) {
        return false;
    }

    uint64_t totalSize = std::accumulate(levels_[level].begin(), levels_[level].end(), uint64_t{0},
                                         [](uint64_t sum, const SSTableMeta &meta) { return sum + meta.sizeBytes; });

    return totalSize > klevelSizes[level];
}

void StorageEngine::saveMetadata() {
    // Called with state_mutex_ held
    std::ofstream metadataFile("data/metadata.txt");
    if (!metadataFile) {
        std::cerr << "Error: Could not open metadata.txt" << '\n';
        return;
    }

    metadataFile << flush_counter_ << '\n';
    metadataFile << seq_number_ << '\n';
    metadataFile.close();

    std::ofstream levelFile("data/levels.txt");
    if (!levelFile) {
        std::cerr << "Error could not open levels.txt" << '\n';
        return;
    }

    for (uint32_t level = 0; level < levels_.size(); level++) {
        for (const auto &meta : levels_[level]) {
            levelFile << meta.id << ' ' << meta.level << ' ' << meta.minKey << ' ' << meta.maxKey << ' ' << meta.maxSeq << ' '
                      << meta.sizeBytes << '\n';
        }
    }
    levelFile.close();
}

void StorageEngine::compactL0toL1() {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);

    if (levels_[0].empty())
        return;

    const std::string dir_path = "data/sstables/";

    std::string minKey = levels_[0][0].minKey;
    std::string maxKey = levels_[0][0].maxKey;
    for (const auto &meta : levels_[0]) {
        if (meta.minKey < minKey)
            minKey = meta.minKey;
        if (meta.maxKey > maxKey)
            maxKey = meta.maxKey;
    }

    std::vector<size_t> l0Indices;
    for (size_t i = 0; i < sstables_.size(); i++) {
        const auto &fname = sstables_[i].filename();

        bool isL0 = std::any_of(levels_[0].begin(), levels_[0].end(), [&](const SSTableMeta &meta) {
            return fname.find("_" + std::to_string(meta.id) + ".") != std::string::npos;
        });

        if (isL0) {
            l0Indices.push_back(i);
        }
    }

    std::vector<size_t> l1SSTIndices;
    std::vector<size_t> l1MetaIndices;

    if (levels_.size() > 1) {
        for (size_t i = 0; i < levels_[1].size(); i++) {
            const auto &meta = levels_[1][i];

            if (!(meta.maxKey < minKey || meta.minKey > maxKey)) {

                auto it = std::find_if(sstables_.begin(), sstables_.end(), [&](const SSTable &table) {
                    return table.filename().find("_" + std::to_string(meta.id) + ".") != std::string::npos;
                });

                if (it != sstables_.end()) {
                    size_t j = std::distance(sstables_.begin(), it);
                    l1SSTIndices.push_back(j);
                    l1MetaIndices.push_back(i);
                }
            }
        }
    } else {
        levels_.resize(std::max(2UL, levels_.size()));
    }

    std::vector<size_t> allIndices = l0Indices;
    allIndices.insert(allIndices.end(), l1SSTIndices.begin(), l1SSTIndices.end());

    std::vector<SSTable::Iterator> iters;
    iters.reserve(allIndices.size());

    std::transform(allIndices.begin(), allIndices.end(), std::back_inserter(iters),
                   [&](size_t idx) { return SSTable::Iterator{sstables_[idx]}; });

    using HeapElement = std::tuple<std::string, uint64_t, EntryType, size_t>;
    auto cmp = [](const HeapElement &a, const HeapElement &b) {
        if (std::get<0>(a) != std::get<0>(b)) {
            return std::get<0>(a) > std::get<0>(b);
        }
        return std::get<1>(a) < std::get<1>(b);
    };

    std::priority_queue<HeapElement, std::vector<HeapElement>, decltype(cmp)> pq(cmp);

    for (size_t i = 0; i < iters.size(); i++) {
        if (iters[i].valid()) {
            const auto &e = iters[i].entry();
            pq.emplace(e.key, e.seq, e.type, i);
        }
    }

    std::map<std::string, Entry> merged_data;

    while (!pq.empty()) {
        auto [key, seq, type, idx] = pq.top();
        pq.pop();

        uint64_t highestSeq = seq;
        EntryType highestType = type;
        std::string highestValue = iters[idx].entry().value;

        std::vector<size_t> sameKeyIndices = {idx};

        while (!pq.empty() && std::get<0>(pq.top()) == key) {
            auto [_, s, t, i] = pq.top();
            pq.pop();
            sameKeyIndices.push_back(i);

            if (s > highestSeq) {
                highestSeq = s;
                highestType = t;
                highestValue = iters[i].entry().value;
            }
        }

        if (highestType == EntryType::PUT) {
            merged_data[key] = Entry{highestValue, highestSeq, highestType};
        }

        for (size_t i : sameKeyIndices) {
            iters[i].next();
            if (iters[i].valid()) {
                const auto &e = iters[i].entry();
                pq.emplace(e.key, e.seq, e.type, i);
            }
        }
    }

    for (const auto &meta : levels_[0]) {
        std::filesystem::remove("data/sstables/sstable_" + std::to_string(meta.id) + ".bin");
    }

    for (size_t idx : l1MetaIndices) {
        std::filesystem::remove("data/sstables/sstable_" + std::to_string(levels_[1][idx].id) + ".bin");
    }

    std::vector<size_t> allIndicesToRemove = allIndices;
    std::sort(allIndicesToRemove.rbegin(), allIndicesToRemove.rend());
    for (size_t idx : allIndicesToRemove) {
        sstables_.erase(sstables_.begin() + idx);
    }

    flush_counter_++;

    SSTable newSSTable = SSTable::flush(merged_data, dir_path, flush_counter_);
    sstables_.push_back(std::move(newSSTable));

    SSTableMeta newMeta;
    newMeta.id = flush_counter_;
    newMeta.level = 1;
    newMeta.minKey = merged_data.begin()->first;
    newMeta.maxKey = merged_data.rbegin()->first;
    newMeta.maxSeq = seq_number_ - 1;
    newMeta.sizeBytes = std::filesystem::file_size(dir_path + "sstable_" + std::to_string(flush_counter_) + ".bin");

    levels_[0].clear();

    std::sort(l1MetaIndices.rbegin(), l1MetaIndices.rend());
    for (size_t idx : l1MetaIndices) {
        levels_[1].erase(levels_[1].begin() + idx);
    }

    levels_[1].push_back(newMeta);
    std::sort(levels_[1].begin(), levels_[1].end(), [](const SSTableMeta &a, const SSTableMeta &b) { return a.minKey < b.minKey; });

    saveMetadata();

    if (cache_) {
        cache_->clear();
    }
}

void StorageEngine::compactlevelN(uint32_t level) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);

    if (level == 0 || level >= levels_.size() - 1)
        return;

    if (levels_[level].empty())
        return;

    const std::string dir_path = "data/sstables/";

    const SSTableMeta &srcMeta = levels_[level][0];
    size_t srcMetaIdx = 0;

    size_t srcSSTIdx = SIZE_MAX;
    auto it = std::find_if(sstables_.begin(), sstables_.end(), [&](const SSTable &table) {
        return table.filename().find("_" + std::to_string(srcMeta.id) + ".") != std::string::npos;
    });

    if (it != sstables_.end()) {
        srcSSTIdx = std::distance(sstables_.begin(), it);
    }

    if (srcSSTIdx == SIZE_MAX) {
        std::cerr << "Error: Could not find SSTable for level " << level << "\n";
        return;
    }

    if (levels_.size() <= level + 1) {
        levels_.resize(level + 2);
    }

    std::vector<size_t> nextLevelSSTIndices;
    std::vector<size_t> nextLevelMetaIndices;

    for (size_t i = 0; i < levels_[level + 1].size(); i++) {
        const auto &meta = levels_[level + 1][i];

        if (!(meta.maxKey < srcMeta.minKey || meta.minKey > srcMeta.maxKey)) {

            auto foundIt = std::find_if(sstables_.begin(), sstables_.end(), [&](const SSTable &table) {
                return table.filename().find("_" + std::to_string(meta.id) + ".") != std::string::npos;
            });

            if (foundIt != sstables_.end()) {
                size_t j = std::distance(sstables_.begin(), foundIt);
                nextLevelSSTIndices.push_back(j);
                nextLevelMetaIndices.push_back(i);
            }
        }
    }

    std::vector<size_t> allIndices = {srcSSTIdx};
    allIndices.insert(allIndices.end(), nextLevelSSTIndices.begin(), nextLevelSSTIndices.end());

    std::vector<SSTable::Iterator> iters;
    iters.reserve(allIndices.size());

    std::transform(allIndices.begin(), allIndices.end(), std::back_inserter(iters),
                   [&](size_t idx) { return SSTable::Iterator{sstables_[idx]}; });

    using HeapElement = std::tuple<std::string, uint64_t, EntryType, size_t>;
    auto cmp = [](const HeapElement &a, const HeapElement &b) {
        if (std::get<0>(a) != std::get<0>(b)) {
            return std::get<0>(a) > std::get<0>(b);
        }
        return std::get<1>(a) < std::get<1>(b);
    };

    std::priority_queue<HeapElement, std::vector<HeapElement>, decltype(cmp)> pq(cmp);

    for (size_t i = 0; i < iters.size(); i++) {
        if (iters[i].valid()) {
            const auto &e = iters[i].entry();
            pq.emplace(e.key, e.seq, e.type, i);
        }
    }

    std::map<std::string, Entry> merged_data;

    while (!pq.empty()) {
        auto [key, seq, type, idx] = pq.top();
        pq.pop();

        uint64_t highestSeq = seq;
        EntryType highestType = type;
        std::string highestValue = iters[idx].entry().value;

        std::vector<size_t> sameKeyIndices = {idx};

        while (!pq.empty() && std::get<0>(pq.top()) == key) {
            auto [_, s, t, i] = pq.top();
            pq.pop();
            sameKeyIndices.push_back(i);

            if (s > highestSeq) {
                highestSeq = s;
                highestType = t;
                highestValue = iters[i].entry().value;
            }
        }

        if (highestType == EntryType::PUT) {
            merged_data[key] = Entry{highestValue, highestSeq, highestType};
        }

        for (size_t i : sameKeyIndices) {
            iters[i].next();
            if (iters[i].valid()) {
                const auto &e = iters[i].entry();
                pq.emplace(e.key, e.seq, e.type, i);
            }
        }
    }

    std::filesystem::remove("data/sstables/sstable_" + std::to_string(srcMeta.id) + ".bin");

    for (size_t idx : nextLevelMetaIndices) {
        std::filesystem::remove("data/sstables/sstable_" + std::to_string(levels_[level + 1][idx].id) + ".bin");
    }

    std::sort(allIndices.rbegin(), allIndices.rend());
    for (size_t idx : allIndices) {
        sstables_.erase(sstables_.begin() + idx);
    }

    flush_counter_++;

    SSTable newSSTable = SSTable::flush(merged_data, dir_path, flush_counter_);
    sstables_.push_back(std::move(newSSTable));

    SSTableMeta newMeta;
    newMeta.id = flush_counter_;
    newMeta.level = level + 1;
    newMeta.minKey = merged_data.begin()->first;
    newMeta.maxKey = merged_data.rbegin()->first;
    newMeta.maxSeq = seq_number_ - 1;
    newMeta.sizeBytes = std::filesystem::file_size(dir_path + "sstable_" + std::to_string(flush_counter_) + ".bin");

    levels_[level].erase(levels_[level].begin() + srcMetaIdx);

    std::sort(nextLevelMetaIndices.rbegin(), nextLevelMetaIndices.rend());
    for (size_t idx : nextLevelMetaIndices) {
        levels_[level + 1].erase(levels_[level + 1].begin() + idx);
    }

    levels_[level + 1].push_back(newMeta);
    std::sort(levels_[level + 1].begin(), levels_[level + 1].end(),
              [](const SSTableMeta &a, const SSTableMeta &b) { return a.minKey < b.minKey; });

    saveMetadata();

    if (cache_) {
        cache_->clear();
    }
}

void StorageEngine::waitForCompaction() {
    // Small delay to make sure that compaction thread has started if it was just scheduled
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Wait until no compaction is in progress
    std::unique_lock<std::mutex> lock(compaction_mutex_);
    compaction_cv_.wait_for(lock, std::chrono::seconds(5), [this] {
        return !compaction_in_progress_.load(std::memory_order_acquire) && !compaction_needed_.load(std::memory_order_acquire);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// cppcheck-suppress unusedFunction
void StorageEngine::pauseCompaction() {
    compaction_paused_.store(true, std::memory_order_release);
}

// cppcheck-suppress unusedFunction
void StorageEngine::resumeCompaction() {
    compaction_paused_.store(false, std::memory_order_release);
    scheduleCompaction();
}
