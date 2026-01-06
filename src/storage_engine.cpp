#include "storage_engine.h"

StorageEngine::StorageEngine(const std::string &wal_path, size_t cache_size) : wal_(wal_path), memtable_(), seq_number_(1) {
    if (cache_size > 0) {
        cache_.emplace(cache_size);
    }

    try {
        std::filesystem::create_directories("data/sstables");
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    std::ifstream metadataFile("data/metadata.txt");

    if (!metadataFile) {
        flush_counter_ = 0;
        auto initialVersion = std::make_shared<TableVersion>();
        initialVersion->levels.resize(4);
        version_manager_.installVersion(initialVersion);
    } else {
        std::string line;
        std::getline(metadataFile, line);
        flush_counter_ = stoull(line);

        std::getline(metadataFile, line);
        seq_number_ = stoull(line);

        loadLevelMetadata();
        loadSSTables();
    }

    flush_thread_ = std::thread(&StorageEngine::flushThreadLoop, this);
    writer_thread_ = std::thread(&StorageEngine::writerThreadLoop, this);
    compaction_thread_ = std::thread(&StorageEngine::compactionThreadLoop, this);
}

StorageEngine::~StorageEngine() {
    writer_shutdown_.store(true, std::memory_order_release);
    write_queue_.shutdown();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(flush_mutex_);
        shutdown_.store(true, std::memory_order_release);
    }
    flush_cv_.notify_one();

    if (flush_thread_.joinable()) {
        flush_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(compaction_mutex_);
    }
    compaction_cv_.notify_one();

    if (compaction_thread_.joinable()) {
        compaction_thread_.join();
    }
}

void StorageEngine::loadLevelMetadata() {
    auto newVersion = std::make_shared<TableVersion>();

    std::ifstream levelFile("data/levels.txt");
    if (!levelFile) {
        newVersion->levels.resize(4);
        version_manager_.installVersion(newVersion);
        return;
    }

    std::string line;

    while (std::getline(levelFile, line)) {
        if (line.empty())
            continue;

        std::istringstream iss(line);
        SSTableMeta meta;
        iss >> meta.id >> meta.level >> meta.minKey >> meta.maxKey >> meta.maxSeq >> meta.sizeBytes;

        if (meta.level >= newVersion->levels.size()) {
            newVersion->levels.resize(meta.level + 1);
        }

        newVersion->levels[meta.level].push_back(meta);
    }

    if (newVersion->levels.empty()) {
        newVersion->levels.resize(4);
    }

    newVersion->flush_counter = flush_counter_;
    version_manager_.installVersion(newVersion);
}

void StorageEngine::loadSSTables() {
    auto newVersion = version_manager_.getVersionForModification();

    for (const auto &levelMetas : newVersion->levels) {
        for (const auto &meta : levelMetas) {
            std::string path = "data/sstables/sstable_" + std::to_string(meta.id) + ".bin";
            if (std::filesystem::exists(path)) {
                newVersion->sstables.push_back(std::make_shared<SSTable>(path));
            } else {
                std::cerr << "Warning: SSTable file was not found: " << path << '\n';
            }
        }
    }

    version_manager_.installVersion(newVersion);
}

bool StorageEngine::put(const std::string &key, const std::string &value) {
    std::future<bool> result = write_queue_.push(Operation::PUT, key, value);
    return result.get();
}

// cppcheck-suppress unusedFunction
std::future<bool> StorageEngine::putAsync(const std::string &key, const std::string &value) {
    return write_queue_.push(Operation::PUT, key, value);
}

bool StorageEngine::del(const std::string &key) {
    Entry existing;
    bool existed = get(key, existing);

    std::future<bool> result = write_queue_.push(Operation::DELETE, key, "");
    result.get();

    return existed;
}

// cppcheck-suppress unusedFunction
std::future<bool> StorageEngine::delAsync(const std::string &key) {
    return write_queue_.push(Operation::DELETE, key, "");
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

    auto immutable = std::atomic_load(&immutable_memtable_);
    if (immutable) {
        Entry immut_mem;
        if (immutable->get(key, immut_mem)) {
            if (!candidate || immut_mem.seq > candidate->seq) {
                candidate = immut_mem;
            }
        }
    }

    auto version = version_manager_.getCurrentVersion();
    const auto &levels = version->levels;

    for (uint32_t level = 0; level < levels.size(); level++) {
        if (levels[level].empty())
            continue;

        if (level == 0) {
            // L0: Have to check all the files since there are overlapping ranges
            for (const auto &meta : levels[0]) {
                auto sst = version->findSSTableById(meta.id);
                if (sst) {
                    std::optional<Entry> record = sst->get(key);
                    if (record && (!candidate || record->seq > candidate->seq)) {
                        candidate = *record;
                    }
                }
            }
        } else {
            // L1+: binary search to find correct file
            auto it = std::lower_bound(levels[level].begin(), levels[level].end(), key,
                                       [](const SSTableMeta &meta, const std::string &k) { return meta.maxKey < k; });

            if (it != levels[level].end() && key >= it->minKey) {
                auto sst = version->findSSTableById(it->id);
                if (sst) {
                    std::optional<Entry> record = sst->get(key);
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
        std::cout << "Memtable (active):\n";
        for (const auto &[k, v] : currentMemtable) {
            std::cout << k << " " << (v.type == EntryType::DELETE ? "<TOMBSTONE>" : v.value) << " " << v.seq << "\n";
        }
        std::cout << '\n';
    }

    auto immutable = std::atomic_load(&immutable_memtable_);
    if (immutable) {
        const auto immutableSnapshot = immutable->snapshot();
        if (!immutableSnapshot.empty()) {
            std::cout << "Memtable (immutable, flushing):\n";
            for (const auto &[k, v] : immutableSnapshot) {
                std::cout << k << " " << (v.type == EntryType::DELETE ? "<TOMBSTONE>" : v.value) << " " << v.seq << "\n";
            }
            std::cout << '\n';
        }
    }

    auto version = version_manager_.getCurrentVersion();

    for (size_t i = version->sstables.size(); i-- > 0;) {
        const auto &sst = version->sstables[i];
        if (!sst)
            continue;

        const std::string path = sst->filename();
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

        std::map<std::string, Entry> currSStableData = sst->getData();
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

        {
            std::unique_lock<std::mutex> lock(flush_mutex_);
            while (std::atomic_load(&immutable_memtable_) && !shutdown_.load(std::memory_order_acquire)) {
                flush_cv_.wait(lock);
            }

            if (shutdown_.load(std::memory_order_acquire)) {
                return;
            }

            auto new_immutable = std::make_shared<MemTable>();
            auto snapshot = memtable_.snapshot();
            for (const auto &[key, entry] : snapshot) {
                if (entry.type == EntryType::PUT) {
                    new_immutable->put(key, entry.value, entry.seq);
                } else {
                    new_immutable->del(key, entry.seq);
                }
            }
            memtable_.clear();

            std::atomic_store(&immutable_memtable_, new_immutable);

            flush_pending_.store(true, std::memory_order_release);
        }

        flush_cv_.notify_one();

        std::remove("data/log.bin");
    }
}

// cppcheck-suppress unusedFunction
void StorageEngine::triggerFlush() {
    checkFlush(true);
}

void StorageEngine::flushThreadLoop() {
    while (true) {
        std::shared_ptr<MemTable> memtable_to_flush;

        {
            std::unique_lock<std::mutex> lock(flush_mutex_);
            flush_cv_.wait(lock,
                           [this] { return shutdown_.load(std::memory_order_acquire) || flush_pending_.load(std::memory_order_acquire); });

            auto current_immutable = std::atomic_load(&immutable_memtable_);
            if (shutdown_.load(std::memory_order_acquire) && !current_immutable) {
                break;
            }

            if (current_immutable) {
                memtable_to_flush = current_immutable;
                flush_pending_.store(false, std::memory_order_release);
            }
        }

        if (memtable_to_flush) {
            std::map<std::string, Entry> snapshot = memtable_to_flush->snapshot();

            if (!snapshot.empty()) {
                const std::string dir_path = "data/sstables/";
                uint64_t new_flush_counter;
                {
                    std::lock_guard<std::mutex> lock(metadata_mutex_);
                    flush_counter_++;
                    new_flush_counter = flush_counter_;
                }

                auto newSSTable = std::make_shared<SSTable>(SSTable::flush(snapshot, dir_path, new_flush_counter));

                SSTableMeta meta;
                meta.id = new_flush_counter;
                meta.level = 0;
                meta.minKey = snapshot.begin()->first;
                meta.maxKey = snapshot.rbegin()->first;
                meta.maxSeq = seq_number_ - 1;
                meta.sizeBytes = std::filesystem::file_size(dir_path + "sstable_" + std::to_string(new_flush_counter) + ".bin");

                auto newVersion = version_manager_.getVersionForModification();
                newVersion->addSSTable(std::move(newSSTable), meta);
                newVersion->flush_counter = new_flush_counter;
                version_manager_.installVersion(newVersion);

                {
                    std::lock_guard<std::mutex> lock(metadata_mutex_);
                    saveMetadata();
                }

                if (cache_) {
                    cache_->clear();
                }

                scheduleCompaction();
            }

            std::atomic_store(&immutable_memtable_, std::shared_ptr<MemTable>(nullptr));

            flush_cv_.notify_all();
        }
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

    memtable_.clear();
    {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        flush_counter_ = 0;
        seq_number_ = 1;
    }

    auto freshVersion = std::make_shared<TableVersion>();
    freshVersion->levels.resize(4);
    version_manager_.installVersion(freshVersion);

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

void StorageEngine::writerThreadLoop() {
    constexpr size_t MAX_BATCH_SIZE = 1000;

    while (true) {
        auto batch = write_queue_.popBatch(MAX_BATCH_SIZE);

        if (batch.empty()) {
            break;
        }

        std::vector<std::pair<WriteRequest *, bool>> results;
        results.reserve(batch.size());

        try {
            for (auto &request : batch) {
                bool success = false;

                switch (request->op) {
                case Operation::PUT:
                    if (memtable_.put(request->key, request->value, seq_number_)) {
                        wal_.append(Operation::PUT, request->key, request->value, seq_number_);
                        seq_number_++;
                        success = true;

                        if (cache_) {
                            cache_->invalidate(request->key);
                        }
                    }
                    break;

                case Operation::DELETE:
                    memtable_.del(request->key, seq_number_);
                    wal_.append(Operation::DELETE, request->key, "", seq_number_);
                    seq_number_++;

                    if (cache_) {
                        cache_->invalidate(request->key);
                    }
                    success = true;
                    break;

                default:
                    break;
                }

                results.emplace_back(request.get(), success);
            }

            wal_.flush();

            checkFlush();

        } catch (const std::exception &e) {
            std::cerr << "Writer thread error: " << e.what() << std::endl;
            for (auto &[req, success] : results) {
                success = false;
            }
        }

        for (auto &[req, success] : results) {
            req->completion.set_value(success);
        }
    }
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
            {
                std::lock_guard<std::mutex> lock(compaction_mutex_);
            }
            compaction_cv_.notify_all();
        }
    }
}

void StorageEngine::maybeCompactBackground() {
    for (uint32_t level = 0; level < 3; level++) {
        auto version = version_manager_.getCurrentVersion();
        bool needs_compaction = shouldCompactUnlocked(level, version);

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
    auto version = version_manager_.getCurrentVersion();
    return shouldCompactUnlocked(level, version);
}

bool StorageEngine::shouldCompactUnlocked(uint32_t level, const std::shared_ptr<TableVersion> &version) {
    if (!version || level >= version->levels.size() || version->levels[level].empty()) {
        return false;
    }

    if (level == 0) {
        return version->levels[0].size() >= 4;
    }

    static const std::vector<uint64_t> klevelSizes = {0, 10 * 1024 * 1024, 100 * 1024 * 1024, 1024 * 1024 * 1024};

    if (level >= klevelSizes.size()) {
        return false;
    }

    uint64_t totalSize = std::accumulate(version->levels[level].begin(), version->levels[level].end(), uint64_t{0},
                                         [](uint64_t sum, const SSTableMeta &meta) { return sum + meta.sizeBytes; });

    return totalSize > klevelSizes[level];
}

void StorageEngine::saveMetadata() {
    auto version = version_manager_.getCurrentVersion();

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

    for (uint32_t level = 0; level < version->levels.size(); level++) {
        for (const auto &meta : version->levels[level]) {
            levelFile << meta.id << ' ' << meta.level << ' ' << meta.minKey << ' ' << meta.maxKey << ' ' << meta.maxSeq << ' '
                      << meta.sizeBytes << '\n';
        }
    }
    levelFile.close();
}

void StorageEngine::compactL0toL1() {
    auto oldVersion = version_manager_.getCurrentVersion();

    if (oldVersion->levels.empty() || oldVersion->levels[0].empty())
        return;

    const std::string dir_path = "data/sstables/";

    std::string minKey = oldVersion->levels[0][0].minKey;
    std::string maxKey = oldVersion->levels[0][0].maxKey;
    for (const auto &meta : oldVersion->levels[0]) {
        if (meta.minKey < minKey)
            minKey = meta.minKey;
        if (meta.maxKey > maxKey)
            maxKey = meta.maxKey;
    }

    std::vector<std::shared_ptr<SSTable>> l0SSTables;
    std::vector<uint64_t> l0Ids;
    for (const auto &meta : oldVersion->levels[0]) {
        auto sst = oldVersion->findSSTableById(meta.id);
        if (sst) {
            l0SSTables.push_back(sst);
            l0Ids.push_back(meta.id);
        }
    }

    std::vector<std::shared_ptr<SSTable>> l1SSTables;
    std::vector<uint64_t> l1Ids;
    if (oldVersion->levels.size() > 1) {
        for (const auto &meta : oldVersion->levels[1]) {
            if (!(meta.maxKey < minKey || meta.minKey > maxKey)) {
                auto sst = oldVersion->findSSTableById(meta.id);
                if (sst) {
                    l1SSTables.push_back(sst);
                    l1Ids.push_back(meta.id);
                }
            }
        }
    }

    std::vector<std::shared_ptr<SSTable>> allSSTables;
    allSSTables.insert(allSSTables.end(), l0SSTables.begin(), l0SSTables.end());
    allSSTables.insert(allSSTables.end(), l1SSTables.begin(), l1SSTables.end());

    std::vector<SSTable::Iterator> iters;
    iters.reserve(allSSTables.size());
    std::transform(allSSTables.begin(), allSSTables.end(), std::back_inserter(iters),
                   [](const std::shared_ptr<SSTable> &sst) { return SSTable::Iterator{*sst}; });

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

    if (merged_data.empty())
        return;

    uint64_t new_flush_counter;
    {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        flush_counter_++;
        new_flush_counter = flush_counter_;
    }

    auto newSSTable = std::make_shared<SSTable>(SSTable::flush(merged_data, dir_path, new_flush_counter));

    SSTableMeta newMeta;
    newMeta.id = new_flush_counter;
    newMeta.level = 1;
    newMeta.minKey = merged_data.begin()->first;
    newMeta.maxKey = merged_data.rbegin()->first;
    newMeta.maxSeq = seq_number_ - 1;
    newMeta.sizeBytes = std::filesystem::file_size(dir_path + "sstable_" + std::to_string(new_flush_counter) + ".bin");

    auto newVersion = version_manager_.getVersionForModification();

    std::vector<uint64_t> idsToRemove;
    idsToRemove.insert(idsToRemove.end(), l0Ids.begin(), l0Ids.end());
    idsToRemove.insert(idsToRemove.end(), l1Ids.begin(), l1Ids.end());
    newVersion->removeSSTablesByIds(idsToRemove);

    newVersion->addSSTable(std::move(newSSTable), newMeta);
    newVersion->flush_counter = new_flush_counter;

    if (newVersion->levels.size() > 1) {
        std::sort(newVersion->levels[1].begin(), newVersion->levels[1].end(),
                  [](const SSTableMeta &a, const SSTableMeta &b) { return a.minKey < b.minKey; });
    }

    version_manager_.installVersion(newVersion);

    {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        saveMetadata();
    }

    for (uint64_t id : idsToRemove) {
        std::filesystem::remove("data/sstables/sstable_" + std::to_string(id) + ".bin");
    }

    if (cache_) {
        cache_->clear();
    }
}

void StorageEngine::compactlevelN(uint32_t level) {
    auto oldVersion = version_manager_.getCurrentVersion();

    if (level == 0 || level >= oldVersion->levels.size())
        return;

    if (oldVersion->levels[level].empty())
        return;

    const std::string dir_path = "data/sstables/";

    const SSTableMeta &srcMeta = oldVersion->levels[level][0];
    auto srcSSTable = oldVersion->findSSTableById(srcMeta.id);

    if (!srcSSTable) {
        std::cerr << "Error: Could not find SSTable for level " << level << "\n";
        return;
    }

    std::vector<std::shared_ptr<SSTable>> nextLevelSSTables;
    std::vector<uint64_t> nextLevelIds;

    if (level + 1 < oldVersion->levels.size()) {
        for (const auto &meta : oldVersion->levels[level + 1]) {
            if (!(meta.maxKey < srcMeta.minKey || meta.minKey > srcMeta.maxKey)) {
                auto sst = oldVersion->findSSTableById(meta.id);
                if (sst) {
                    nextLevelSSTables.push_back(sst);
                    nextLevelIds.push_back(meta.id);
                }
            }
        }
    }

    std::vector<std::shared_ptr<SSTable>> allSSTables;
    allSSTables.push_back(srcSSTable);
    allSSTables.insert(allSSTables.end(), nextLevelSSTables.begin(), nextLevelSSTables.end());

    std::vector<SSTable::Iterator> iters;
    iters.reserve(allSSTables.size());
    std::transform(allSSTables.begin(), allSSTables.end(), std::back_inserter(iters),
                   [](const std::shared_ptr<SSTable> &sst) { return SSTable::Iterator{*sst}; });

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

    if (merged_data.empty())
        return;

    uint64_t new_flush_counter;
    {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        flush_counter_++;
        new_flush_counter = flush_counter_;
    }

    auto newSSTable = std::make_shared<SSTable>(SSTable::flush(merged_data, dir_path, new_flush_counter));

    SSTableMeta newMeta;
    newMeta.id = new_flush_counter;
    newMeta.level = level + 1;
    newMeta.minKey = merged_data.begin()->first;
    newMeta.maxKey = merged_data.rbegin()->first;
    newMeta.maxSeq = seq_number_ - 1;
    newMeta.sizeBytes = std::filesystem::file_size(dir_path + "sstable_" + std::to_string(new_flush_counter) + ".bin");

    auto newVersion = version_manager_.getVersionForModification();

    if (newVersion->levels.size() <= level + 1) {
        newVersion->levels.resize(level + 2);
    }

    std::vector<uint64_t> idsToRemove = {srcMeta.id};
    idsToRemove.insert(idsToRemove.end(), nextLevelIds.begin(), nextLevelIds.end());
    newVersion->removeSSTablesByIds(idsToRemove);

    newVersion->addSSTable(std::move(newSSTable), newMeta);
    newVersion->flush_counter = new_flush_counter;

    std::sort(newVersion->levels[level + 1].begin(), newVersion->levels[level + 1].end(),
              [](const SSTableMeta &a, const SSTableMeta &b) { return a.minKey < b.minKey; });

    version_manager_.installVersion(newVersion);

    {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        saveMetadata();
    }

    for (uint64_t id : idsToRemove) {
        std::filesystem::remove("data/sstables/sstable_" + std::to_string(id) + ".bin");
    }

    if (cache_) {
        cache_->clear();
    }
}

void StorageEngine::waitForCompaction() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
