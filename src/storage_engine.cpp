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
    bool result = false;
    if (memtable_.del(key)) {
        wal_.append(Operation::DELETE, key, "", seq_number_);
        seq_number_++;
        result = true;
    }
    checkFlush();
    return result;
}

bool StorageEngine::get(const std::string &key, std::string &out) const {
    bool result = false;
    if (!memtable_.get(key, out)) {
        for (size_t i = sstables_.size(); i-- > 0;) {
            std::optional<std::string> sstableResult = sstables_[i].get(key);
            if (sstableResult != std::nullopt) {
                out = *sstableResult;
                result = true;
                return result;
            }
        }
    } else {
        result = true;
    }
    return result;
}

void StorageEngine::ls() const {
    const std::map<std::string, Entry> currentMemtable = memtable_.snapshot();
    if (!currentMemtable.empty()) {
        std::cout << "Memtable:\n";
        for (const auto &[k, v] : currentMemtable) {
            std::cout << k << " " << v.value << " " << v.seq << std::endl;
        }
        std::cout << '\n';
    }
    if (!sstables_.empty()) {
        for (size_t i = sstables_.size(); i-- > 0;) {
            std::cout << "SSTable " << i << ":\n";
            std::map<std::string, Entry> currSStableData = sstables_[i].getData();
            for (const auto &[k, v] : currSStableData) {
                std::cout << k << " " << v.value << " " << v.seq << std::endl;
            }
            std::cout << '\n';
        }
    }
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
                memtable_.del(key);
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
        std::string result;
        get(key, result);
        std::cout << result << std::endl;
        break;
    }
    case Operation::LS: {
        ls();
        break;
    }
    case Operation::DELETE:
        del(key);
        break;
    default:
        std::cerr << "Invalid command\n";
    }
}

void StorageEngine::checkFlush() {
    static constexpr size_t kMemTableThreshold = 8 * 1024 * 1024;
    // Check if memtable is greater than 8MB
    if (memtable_.getSize() >= kMemTableThreshold) {
        std::map<std::string, Entry> currentMemTable;

        std::cout << "DEBUG: Memtable is greater than threshold\n";

        for (const auto &[k, v] : memtable_.snapshot()) {
            currentMemTable[k] = {v.value, v.seq};
        }

        const std::string dir_path = "data/sstables/";

        flush_counter_++;

        SSTable newSSTable = SSTable::flush(currentMemTable, dir_path, flush_counter_);
        sstables_.push_back(newSSTable);

        std::remove("data/log.bin");

        std::ofstream metadataFile("data/metadata.txt");
        metadataFile << flush_counter_ << '\n';
        metadataFile << seq_number_;

        memtable_.clear();
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
