#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <zlib.h>

enum class Operation { GET = 0, PUT = 1, DELETE = 2, ERROR = 3 };

// Calculate checksum using zlib
uint32_t calculateChecksum(Operation op, const std::string &key, const std::string &value) {
    const int OP_BYTES = 1;
    const int KEY_LEN_BYTES = 4;
    const int VALUE_LEN_BYTES = 4;

    uint32_t keyLen = key.size();
    uint32_t valueLen = value.size();
    size_t totalSize = OP_BYTES + KEY_LEN_BYTES + VALUE_LEN_BYTES + keyLen + valueLen;
    Bytef *buffer = new Bytef[totalSize];

    size_t offset = 0;

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

// Parse input commands like: put("key", "value"), get("key"), delete("key")
Operation parseCommand(const std::string &input, std::string &key, std::string &value) {
    key.clear();
    value.clear();

    if (input.substr(0, 4) == "put(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);

        size_t startValue = input.find('"', endKey + 1);
        size_t endValue = input.find('"', startValue + 1);
        if (startValue == std::string::npos || endValue == std::string::npos)
            return Operation::ERROR;

        value = input.substr(startValue + 1, endValue - startValue - 1);
        return Operation::PUT;

    } else if (input.substr(0, 4) == "get(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);
        return Operation::GET;

    } else if (input.substr(0, 7) == "delete(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);
        return Operation::DELETE;
    }

    return Operation::ERROR;
}

// Write log inside binary file
void writeLog(Operation op, const std::string &key, const std::string &value) {
    uint32_t checksum = calculateChecksum(op, key, value);
    uint16_t keyLen = key.size();
    uint16_t valueLen = value.size();
    uint16_t keyLenBytes = key.size();
    uint16_t valueLenBytes = value.size();
    uint8_t opByte = static_cast<uint8_t>(op);
    const char *keyBytes = key.data();
    const char *valueBytes = value.data();

    std::ofstream logFile("log.bin", std::ios::app | std::ios::binary);
    if (!logFile) {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }

    logFile.write(reinterpret_cast<const char *>(&checksum), sizeof(checksum));
    logFile.write(reinterpret_cast<const char *>(&opByte), sizeof(opByte));
    logFile.write(reinterpret_cast<const char *>(&keyLenBytes), sizeof(keyLenBytes));
    logFile.write(reinterpret_cast<const char *>(&valueLenBytes), sizeof(valueLenBytes));
    logFile.write(keyBytes, keyLen);
    logFile.write(valueBytes, valueLen);
}

// Load log and rebuild memtable
void loadLog(const std::string &fileName, const std::map<std::string, std::string> &memtable) {
    std::ifstream inputFile(fileName, std::ios::in | std::ios::binary);

    if (!inputFile) {
        return;
    }

    uint32_t checksum{};
    uint8_t opByte{};
    uint16_t keyLenBytes{};
    uint16_t valueLenBytes{};

    while (true) {
        if (!inputFile.read(reinterpret_cast<char *>(&checksum), sizeof(checksum))) {
            break;
        }
        inputFile.read(reinterpret_cast<char *>(&opByte), sizeof(opByte));
        inputFile.read(reinterpret_cast<char *>(&keyLenBytes), sizeof(keyLenBytes));
        inputFile.read(reinterpret_cast<char *>(&valueLenBytes), sizeof(valueLenBytes));

        Operation op = static_cast<Operation>(opByte);
        uint16_t keyLen = static_cast<uint16_t>(keyLenBytes);
        uint16_t valueLen = static_cast<uint16_t>(valueLenBytes);

        std::string key;
        std::string value;
        key.resize(keyLen);
        value.resize(valueLen);

        inputFile.read(reinterpret_cast<char *>(&key[0]), keyLen);
        inputFile.read(reinterpret_cast<char *>(&value[0]), valueLen);

        std::cout << checksum << std::endl;
        std::cout << static_cast<int>(op) << std::endl;
        std::cout << keyLen << std::endl;
        std::cout << valueLen << std::endl;
        std::cout << key << std::endl;
        std::cout << value << std::endl;
    }
}

int main() {
    std::map<std::string, std::string> memtable;
    loadLog("log.bin", memtable);

    std::string input;
    while (true) {
        std::cout << "> ";

        std::string input;
        std::getline(std::cin, input);

        std::string key, value;
        Operation op = parseCommand(input, key, value);

        if (op != Operation::GET && op != Operation::ERROR) {
            writeLog(op, key, value);
        }

        switch (op) {
        case Operation::PUT:
            memtable[key] = value;
            break;

        case Operation::DELETE:
            memtable.erase(key);
            break;

        case Operation::GET:
            if (memtable.contains(key))
                std::cout << memtable[key] << std::endl;
            else
                std::cout << "Key not found\n";
            break;

        case Operation::ERROR:
        default:
            std::cerr << "Invalid command" << std::endl;
        }
    }
}
