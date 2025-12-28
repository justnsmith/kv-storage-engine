#include "sstable.h"

// TODO: Hot keys cache implementation

SSTable::SSTable(const std::string &path) : path_(path) {
    loadMetadata();
}

SSTable SSTable::flush(const std::map<std::string, Entry> &snapshot, const std::string &dir_path, uint64_t flush_counter) {
    std::string full_path = dir_path + "sstable_" + std::to_string(flush_counter) + ".bin";
    std::filesystem::create_directories(dir_path);
    SSTable table(full_path);

    std::ofstream sstableFile(full_path, std::ios::out | std::ios::binary);
    if (!sstableFile) {
        throw std::runtime_error("Failed to open SSTable file: " + full_path);
    }

    for (const auto &[k, v] : snapshot) {
        uint32_t keyLen = k.size();
        uint32_t valueLen = v.value.size();
        const char *keyBytes = k.data();
        const char *valueBytes = v.value.data();

        sstableFile.write(reinterpret_cast<const char *>(&v.seq), sizeof(v.seq));
        sstableFile.write(reinterpret_cast<const char *>(&keyLen), sizeof(keyLen));
        sstableFile.write(reinterpret_cast<const char *>(&valueLen), sizeof(valueLen));
        sstableFile.write(keyBytes, keyLen);
        sstableFile.write(valueBytes, valueLen);
    }

    table.min_key_ = snapshot.begin()->first;
    table.max_key_ = snapshot.rbegin()->first;

    uint32_t minKeyLen = table.min_key_.size();
    uint32_t maxKeyLen = table.max_key_.size();

    table.metadata_offset_ = sstableFile.tellp();

    sstableFile.write(reinterpret_cast<const char *>(&minKeyLen), sizeof(minKeyLen));
    sstableFile.write(reinterpret_cast<const char *>(&maxKeyLen), sizeof(maxKeyLen));

    sstableFile.write(table.min_key_.data(), minKeyLen);
    sstableFile.write(table.max_key_.data(), maxKeyLen);

    sstableFile.write(reinterpret_cast<const char *>(&table.metadata_offset_), sizeof(table.metadata_offset_));

    return table;
}

std::optional<std::string> SSTable::get(const std::string &key) const {
    std::cout << min_key_ << " " << max_key_ << std::endl;
    if (key < min_key_ || key > max_key_) {
        std::cout << "here5";
        return std::nullopt;
    }

    std::ifstream sstableFile(path_, std::ios::in | std::ios::binary);

    if (!sstableFile) {
        throw std::runtime_error("Failed to open SSTable file: " + path_);
    }

    uint64_t seqNumBytes{};
    uint32_t keyLenBytes{};
    uint32_t valueLenBytes{};

    while (true) {

        if (!sstableFile.read(reinterpret_cast<char *>(&seqNumBytes), sizeof(seqNumBytes))) {
            break;
        }
        sstableFile.read(reinterpret_cast<char *>(&keyLenBytes), sizeof(keyLenBytes));
        sstableFile.read(reinterpret_cast<char *>(&valueLenBytes), sizeof(valueLenBytes));

        // uint64_t seqNum = static_cast<uint32_t>(seqNumBytes);
        uint32_t keyLen = static_cast<uint32_t>(keyLenBytes);
        uint32_t valueLen = static_cast<uint32_t>(valueLenBytes);

        std::string currKey;
        std::string currValue;
        currKey.resize(keyLen);
        currValue.resize(valueLen);

        sstableFile.read(reinterpret_cast<char *>(&currKey[0]), keyLen);
        sstableFile.read(reinterpret_cast<char *>(&currValue[0]), valueLen);

        std::cout << "KEYS: " << currKey << " " << key << std::endl;
        if (currKey == key) {
            return currValue;
        }
    }
    return std::nullopt;
}

std::map<std::string, Entry> SSTable::getData() const {
    std::map<std::string, Entry> sstableData;

    std::ifstream sstableFile(path_, std::ios::in | std::ios::binary);

    if (!sstableFile) {
        throw std::runtime_error("Failed to open SSTable file: " + path_);
    }

    uint64_t seqNumBytes{};
    uint32_t keyLenBytes{};
    uint32_t valueLenBytes{};

    while (true) {
        if (!sstableFile.read(reinterpret_cast<char *>(&seqNumBytes), sizeof(seqNumBytes)) ||
            !sstableFile.read(reinterpret_cast<char *>(&keyLenBytes), sizeof(keyLenBytes)) ||
            !sstableFile.read(reinterpret_cast<char *>(&valueLenBytes), sizeof(valueLenBytes))) {
            break;
        }

        uint64_t seqNum = static_cast<uint32_t>(seqNumBytes);
        uint32_t keyLen = static_cast<uint32_t>(keyLenBytes);
        uint32_t valueLen = static_cast<uint32_t>(valueLenBytes);

        std::string currKey;
        std::string currValue;
        currKey.resize(keyLen);
        currValue.resize(valueLen);

        if (!sstableFile.read(reinterpret_cast<char *>(&currKey[0]), keyLen) ||
            !sstableFile.read(reinterpret_cast<char *>(&currValue[0]), valueLen)) {
            break;
        }

        sstableData[currKey].value = currValue;
        sstableData[currKey].seq = seqNum;
    }
    return sstableData;
}

void SSTable::loadMetadata() {
    std::ifstream sstableFile(path_, std::ios::binary);
    if (!sstableFile)
        return;

    sstableFile.seekg(-static_cast<int>(sizeof(uint64_t)), std::ios::end);

    sstableFile.read(reinterpret_cast<char *>(&metadata_offset_), sizeof(metadata_offset_));

    sstableFile.seekg(metadata_offset_);

    uint32_t minKeyLen, maxKeyLen;
    sstableFile.read(reinterpret_cast<char *>(&minKeyLen), sizeof(minKeyLen));
    min_key_.resize(minKeyLen);
    sstableFile.read(reinterpret_cast<char *>(&maxKeyLen), sizeof(maxKeyLen));
    max_key_.resize(maxKeyLen);

    sstableFile.read(&min_key_[0], minKeyLen);
    sstableFile.read(&max_key_[0], maxKeyLen);

    std::cout << "MIN KEY: " << min_key_ << std::endl;
    std::cout << "MAX KEY: " << max_key_ << std::endl;
}
