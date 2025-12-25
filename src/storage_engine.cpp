#include "storage_engine.h"

StorageEngine::StorageEngine(const std::string &wal_path) : wal_(wal_path), memtable_() {
    std::ifstream metadataFile("data/storage_metadata.txt");

    if (!metadataFile) {
        flush_counter_ = 0;
    } else {
        std::string line;
        std::getline(metadataFile, line);
        uint64_t flush_counter = stoull(line);
        flush_counter_ = flush_counter;
    }

    for (uint64_t i = 1; i <= flush_counter_; i++) {
        std::string path = "data/sstables/sstable_" + std::to_string(i) + ".bin";
        sstables_.emplace_back(path);
    }
}

void StorageEngine::put(const std::string &key, const std::string &value) {
    if (memtable_.put(key, value))
        wal_.append(Operation::PUT, key, value);
    std::cout << "Checking size: " << " " << memtable_.getSize() << '\n';
    checkFlush();
}

void StorageEngine::del(const std::string &key) {
    if (memtable_.del(key))
        wal_.append(Operation::DELETE, key, "");
    std::cout << "Checking size: " << " " << memtable_.getSize() << '\n';
    checkFlush();
}

void StorageEngine::get(const std::string &key, std::string &out) const {
    if (!memtable_.get(key, out)) {
        for (size_t i = sstables_.size(); i-- > 0;) {
            std::optional<std::string> result = sstables_[i].get(key);
            if (result != std::nullopt) {
                out = *result;
                return;
            }
        }
    }
}

void StorageEngine::ls() const {
    const std::map<std::string, std::string> currentMemtable = memtable_.snapshot();
    if (!currentMemtable.empty()) {
        std::cout << "Memtable:\n";
        for (const auto &[key, value] : currentMemtable) {
            std::cout << key << " " << value << std::endl;
        }
        std::cout << '\n';
    }
    if (!sstables_.empty()) {
        for (size_t i = sstables_.size(); i-- > 0;) {
            std::cout << "SSTable " << i << ":\n";
            std::map<std::string, std::string> currSStableData = sstables_[i].getData();
            for (const auto &[key, value] : currSStableData) {
                std::cout << key << " " << value << std::endl;
            }
            std::cout << '\n';
        }
    }
}

void StorageEngine::recover() {
    wal_.replay([this](Operation op, const std::string &key, const std::string &value) {
        switch (op) {
        case Operation::PUT:
            memtable_.put(key, value);
            break;
        case Operation::DELETE:
            memtable_.del(key);
            break;
        default:
            std::cerr << "Error reading operation\n";
        }
    });
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
    static constexpr size_t kMemTableThreshold = 50;
    // Check if memtable is greater than 8MB
    if (memtable_.getSize() >= kMemTableThreshold) {

        std::cout << "Memtable is greater than threshold\n";

        const std::map<std::string, std::string> currentMemtable = memtable_.snapshot();
        const std::string dir_path = "data/sstables/";

        flush_counter_++;

        std::cout << "Flushing...\n";

        SSTable newSSTable = SSTable::flush(currentMemtable, dir_path, flush_counter_);
        sstables_.push_back(newSSTable);

        std::ofstream metadataFile("data/storage_metadata.txt");
        metadataFile << flush_counter_;

        memtable_.clear();
    }
}
