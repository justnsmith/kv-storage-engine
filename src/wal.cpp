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
    std::unique_ptr<Bytef[]> buffer(new Bytef[totalSize]);

    size_t offset = 0;

    std::memcpy(buffer.get() + offset, &seqNumber, 8);
    offset += 8;

    buffer[offset++] = static_cast<uint8_t>(op);

    std::memcpy(buffer.get() + offset, &keyLen, KEY_LEN_BYTES);
    offset += 4;

    std::memcpy(buffer.get() + offset, &valueLen, VALUE_LEN_BYTES);
    offset += 4;

    std::memcpy(buffer.get() + offset, key.data(), keyLen);
    offset += keyLen;

    std::memcpy(buffer.get() + offset, value.data(), valueLen);
    offset += valueLen;

    uint32_t crc = crc32(0L, buffer.get(), offset);

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

    logFile.flush();

    int fd = open(path_.c_str(), O_WRONLY);
    if (fd != -1) {
        if (fsync(fd) != 0) {
            std::cerr << "fsync failed: " << strerror(errno) << std::endl;
        }
        close(fd);
    } else {
        std::cerr << "Failed to open file for fsync\n";
    }
}

void WriteAheadLog::replay(std::function<void(uint64_t, Operation, std::string &, std::string &)> apply) {
    std::ifstream inputFile(path_, std::ios::in | std::ios::binary);

    if (!inputFile) {
        return;
    }

    uint32_t checksum{};
    uint64_t seqNumber{};
    uint8_t opByte{};
    uint32_t keyLen{};
    uint32_t valueLen{};

    while (inputFile.read(reinterpret_cast<char *>(&checksum), sizeof(checksum))) {
        inputFile.read(reinterpret_cast<char *>(&seqNumber), sizeof(seqNumber));
        inputFile.read(reinterpret_cast<char *>(&opByte), sizeof(opByte));
        inputFile.read(reinterpret_cast<char *>(&keyLen), sizeof(keyLen));
        inputFile.read(reinterpret_cast<char *>(&valueLen), sizeof(valueLen));

        Operation op = static_cast<Operation>(opByte);

        std::string key(keyLen, '\0');
        std::string value(valueLen, '\0');

        inputFile.read(&key[0], keyLen);
        inputFile.read(&value[0], valueLen);

        uint32_t newChecksum = calculateChecksum(op, key, value, seqNumber);
        if (newChecksum == checksum) {
            apply(seqNumber, op, key, value);
        } else {
            std::cerr << "Data has been corrupted\n";
            break;
        }
    }
}

bool WriteAheadLog::empty() const {
    if (!std::filesystem::exists(path_)) {
        return true;
    }
    return std::filesystem::file_size(path_) == 0;
}
