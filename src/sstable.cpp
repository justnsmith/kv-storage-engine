#include "sstable.h"

// TODO: Hot keys cache implementation

SSTable::SSTable(const std::string &path) : path_(path) {
}

SSTable SSTable::flush(const std::map<std::string, std::string> &snapshot, const std::string &dir_path, uint64_t flush_counter) {
    std::string full_path = dir_path + "sstable_" + std::to_string(flush_counter) + ".bin";
    std::filesystem::create_directories(dir_path);

    std::ofstream sstableFile(full_path, std::ios::out | std::ios::binary);
    if (!sstableFile) {
        throw std::runtime_error("Failed to open SSTable file: " + full_path);
    }

    for (const auto &[key, value] : snapshot) {
        uint16_t keyLen = key.size();
        uint16_t valueLen = value.size();
        const char *keyBytes = key.data();
        const char *valueBytes = value.data();

        sstableFile.write(reinterpret_cast<const char *>(&keyLen), sizeof(keyLen));
        sstableFile.write(reinterpret_cast<const char *>(&valueLen), sizeof(valueLen));
        sstableFile.write(keyBytes, keyLen);
        sstableFile.write(valueBytes, valueLen);
    }
    SSTable result(full_path);
    result.min_key_ = snapshot.begin()->first;
    result.max_key_ = snapshot.rbegin()->first;
    return result;
}

std::optional<std::string> SSTable::get(const std::string &key) const {
    if (key < min_key_ || key > max_key_)
        return std::nullopt;

    std::ifstream sstableFile(path_, std::ios::in | std::ios::binary);

    if (!sstableFile) {
        throw std::runtime_error("Failed to open SSTable file: " + path_);
    }

    uint16_t keyLenBytes{};
    uint16_t valueLenBytes{};

    while (true) {
        if (!sstableFile.read(reinterpret_cast<char *>(&keyLenBytes), sizeof(keyLenBytes))) {
            break;
        }
        sstableFile.read(reinterpret_cast<char *>(&valueLenBytes), sizeof(valueLenBytes));

        uint16_t keyLen = static_cast<uint16_t>(keyLenBytes);
        uint16_t valueLen = static_cast<uint16_t>(valueLenBytes);

        std::string currKey;
        std::string currValue;
        currKey.resize(keyLen);
        currValue.resize(valueLen);

        sstableFile.read(reinterpret_cast<char *>(&currKey[0]), keyLen);
        sstableFile.read(reinterpret_cast<char *>(&currValue[0]), valueLen);

        if (currKey == key) {
            return currValue;
        }
    }
    return std::nullopt;
}

std::map<std::string, std::string> SSTable::getData() const {
    std::map<std::string, std::string> sstableData;

    std::ifstream sstableFile(path_, std::ios::in | std::ios::binary);

    if (!sstableFile) {
        throw std::runtime_error("Failed to open SSTable file: " + path_);
    }

    uint16_t keyLenBytes{};
    uint16_t valueLenBytes{};

    while (true) {
        if (!sstableFile.read(reinterpret_cast<char *>(&keyLenBytes), sizeof(keyLenBytes))) {
            break;
        }
        sstableFile.read(reinterpret_cast<char *>(&valueLenBytes), sizeof(valueLenBytes));

        uint16_t keyLen = static_cast<uint16_t>(keyLenBytes);
        uint16_t valueLen = static_cast<uint16_t>(valueLenBytes);

        std::string currKey;
        std::string currValue;
        currKey.resize(keyLen);
        currValue.resize(valueLen);

        sstableFile.read(reinterpret_cast<char *>(&currKey[0]), keyLen);
        sstableFile.read(reinterpret_cast<char *>(&currValue[0]), valueLen);

        sstableData[currKey] = currValue;
    }
    return sstableData;
}
