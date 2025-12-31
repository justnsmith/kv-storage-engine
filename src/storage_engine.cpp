#include "storage_engine.h"

StorageEngine::StorageEngine(const std::string &wal_path) : wal_(wal_path), memtable_(), seq_number_(1) {
    std::ifstream metadataFile("data/metadata.txt");

    if (!metadataFile) {
        flush_counter_ = 0;
    } else {
        std::string line;
        std::getline(metadataFile, line);
        uint64_t flush_counter = stoull(line);
        flush_counter_ = flush_counter;

        std::getline(metadataFile, line);
        uint64_t seqNumber = stoull(line);
        seq_number_ = seqNumber;
    }

    for (uint64_t i = 1; i <= flush_counter_; i++) {
        std::string path = "data/sstables/sstable_" + std::to_string(i) + ".bin";
        sstables_.emplace_back(path);
    }
}

bool StorageEngine::put(const std::string &key, const std::string &value) {
    bool result = false;
    if (memtable_.put(key, value, seq_number_)) {
        wal_.append(Operation::PUT, key, value, seq_number_);
        seq_number_++;
        result = true;
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

    checkFlush();

    std::cout << std::boolalpha << existed << std::endl;
    return existed;
}

bool StorageEngine::get(const std::string &key, Entry &out) const {
    std::optional<Entry> candidate{};

    Entry mem;
    if (memtable_.get(key, mem)) {
        candidate = mem;
    }

    for (size_t i = sstables_.size(); i-- > 0;) {
        std::optional<Entry> record = sstables_[i].get(key);
        if (record != std::nullopt) {
            if (!candidate || record->seq > candidate->seq) {
                candidate = *record;
            }
            break;
        }
    }

    if (!candidate)
        return false;

    if (candidate->type == EntryType::DELETE) {
        return false;
    }

    out = *candidate;
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

    for (size_t i = sstables_.size(); i-- > 0;) {
        const std::string path = sstables_[i].filename();
        if (!std::filesystem::exists(path)) {
            std::cerr << "Warning: SSTable file missing: " << path << ", skipping.\n";
            continue; // Skip missing files
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

void StorageEngine::clear() {
    std::filesystem::remove_all("data");
    memtable_.clear();
    sstables_.clear();
    flush_counter_ = 0;
    seq_number_ = 1;
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
        std::cout << "here" << std::endl;
        seq_number_ = maxSeqNumber + 1;
    }
    std::cout << seq_number_ << " " << maxSeqNumber << std::endl;
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
        clear();
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
    // Check if memtable is greater than 8MB
    if (memtable_.getSize() >= kMemTableThreshold || debug) {
        std::map<std::string, Entry> currentMemTable;

        std::cout << "DEBUG: Memtable is greater than threshold\n";

        for (const auto &[k, v] : memtable_.snapshot()) {
            currentMemTable[k] = v;
        }

        const std::string dir_path = "data/sstables/";

        flush_counter_++;

        SSTable newSSTable = SSTable::flush(currentMemTable, dir_path, flush_counter_);
        sstables_.push_back(std::move(newSSTable));

        std::remove("data/log.bin");

        std::ofstream metadataFile("data/metadata.txt");
        metadataFile << flush_counter_ << '\n';
        metadataFile << seq_number_;

        memtable_.clear();
        checkCompaction();
    }
}

void StorageEngine::clearData() {
    std::filesystem::path dataPath = "data";
    std::filesystem::create_directories(dataPath);

    try {
        if (std::filesystem::remove_all(dataPath) > 0) {
            memtable_.clear();
            // std::cout << "Memory successfuly cleared" << std::endl;
        } else {
            std::cerr << "The folder was not found or something went wrong" << std::endl;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
}

void StorageEngine::checkCompaction() {
    // Only compact if there are 4 or more SStables
    if (sstables_.size() < 4) {
        return;
    }

    std::cout << "Merging...\n";

    // Collect all of the merged data
    std::map<std::string, Entry> merged_data;

    std::vector<SSTable::Iterator> iters;
    std::transform(sstables_.begin(), sstables_.end(), std::back_inserter(iters),
                   [](const auto &sstable) { return SSTable::Iterator(sstable); });

    // Priority queue element: (key, seq, iterator index)
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

    // Remove the old SStables that were compacted
    for (const auto &sstable : sstables_) {
        std::filesystem::remove(sstable.filename());
    }

    // Use SSTable::flush to write with the bloom filter and index
    const std::string dir_path = "data/sstables/";
    flush_counter_++;

    SSTable newSSTable = SSTable::flush(merged_data, dir_path, flush_counter_);

    sstables_.clear();
    sstables_.push_back(std::move(newSSTable));

    // Update metadata
    std::ofstream metadataFile("data/metadata.txt");
    metadataFile << flush_counter_ << '\n';
    metadataFile << seq_number_;
}
