#include "wal.h"

WriteAheadLog::WriteAheadLog(const std::string &path) : path_(path) {
}

uint32_t WriteAheadLog::calculateChecksum(Operation op, const std::string &key, const std::string &value, const uint64_t seqNumber) {
    const int SEQ_NUM_BYTES = 8;
    const int OP_BYTES = 1;
    const int KEY_LEN_BYTES = 4;
    const int VALUE_LEN_BYTES = 4;

    uint32_t keyLen = key.size();
    uint32_t valueLen = value.size();
    size_t totalSize = SEQ_NUM_BYTES + OP_BYTES + KEY_LEN_BYTES + VALUE_LEN_BYTES + keyLen + valueLen;
    Bytef *buffer = new Bytef[totalSize];

    size_t offset = 0;

    std::memcpy(buffer + offset, &seqNumber, 8);
    offset += 8;

    buffer[offset++] = static_cast<uint8_t>(op);

    std::memcpy(buffer + offset, &keyLen, 4);
    offset += 4;

    std::memcpy(buffer + offset, &valueLen, 4);
    offset += 4;

    std::memcpy(buffer + offset, key.data(), keyLen);
    offset += keyLen;

    std::memcpy(buffer + offset, value.data(), valueLen);
    offset += valueLen;

    std::cout << "Total data bytes in buffer: " << offset << std::endl;

    uint32_t crc = crc32(0L, buffer, offset);
    std::cout << "CRC32: 0x" << crc << std::endl;

    delete[] buffer;
    return crc;
}

void WriteAheadLog::append(Operation op, const std::string &key, const std::string &value, uint64_t seqNumber) {
    uint32_t checksum = calculateChecksum(op, key, value, seqNumber);
    uint32_t keyLen = key.size();
    uint32_t valueLen = value.size();
    uint8_t opByte = static_cast<uint8_t>(op);
    const char *keyBytes = key.data();
    const char *valueBytes = value.data();

    std::filesystem::path p(path_);
    std::filesystem::create_directories(p.parent_path());

    std::ofstream logFile(path_, std::ios::app | std::ios::binary);
    if (!logFile) {
        std::cerr << "Failed to open log file" << std::endl;
        return;
    }

    logFile.write(reinterpret_cast<const char *>(&checksum), sizeof(checksum));
    logFile.write(reinterpret_cast<const char *>(&seqNumber), sizeof(seqNumber));
    logFile.write(reinterpret_cast<const char *>(&opByte), sizeof(opByte));
    logFile.write(reinterpret_cast<const char *>(&keyLen), sizeof(keyLen));
    logFile.write(reinterpret_cast<const char *>(&valueLen), sizeof(valueLen));
    logFile.write(keyBytes, keyLen);
    logFile.write(valueBytes, valueLen);
}

void WriteAheadLog::replay(std::function<void(uint64_t, Operation, std::string &, std::string &)> apply) {
    std::ifstream inputFile(path_, std::ios::in | std::ios::binary);

    if (!inputFile) {
        return;
    }

    uint32_t checksum{};
    uint64_t seqNumberBytes{};
    uint8_t opByte{};
    uint32_t keyLenBytes{};
    uint32_t valueLenBytes{};

    while (true) {
        if (!inputFile.read(reinterpret_cast<char *>(&checksum), sizeof(checksum))) {
            break;
        }
        inputFile.read(reinterpret_cast<char *>(&seqNumberBytes), sizeof(seqNumberBytes));
        inputFile.read(reinterpret_cast<char *>(&opByte), sizeof(opByte));
        inputFile.read(reinterpret_cast<char *>(&keyLenBytes), sizeof(keyLenBytes));
        inputFile.read(reinterpret_cast<char *>(&valueLenBytes), sizeof(valueLenBytes));

        uint64_t seqNumber = static_cast<uint64_t>(seqNumberBytes);
        Operation op = static_cast<Operation>(opByte);
        uint32_t keyLen = static_cast<uint32_t>(keyLenBytes);
        uint32_t valueLen = static_cast<uint32_t>(valueLenBytes);

        std::string key;
        std::string value;
        key.resize(keyLen);
        value.resize(valueLen);

        inputFile.read(reinterpret_cast<char *>(&key[0]), keyLen);
        inputFile.read(reinterpret_cast<char *>(&value[0]), valueLen);

        std::cout << "Cheksum: " << checksum << std::endl;
        std::cout << "Sequence Number: " << seqNumber << std::endl;
        std::cout << "Operation: " << static_cast<int>(op) << std::endl;
        std::cout << "Key Length: " << keyLen << std::endl;
        std::cout << "Value Length: " << valueLen << std::endl;
        std::cout << "Key: " << key << std::endl;
        std::cout << "Value: " << value << std::endl;

        uint32_t newChecksum = calculateChecksum(op, key, value, seqNumber);
        if (newChecksum == checksum) {
            apply(seqNumber, op, key, value);
        } else {
            std::cerr << "Data has been corrupted\n";
            break;
        }
    }
}
