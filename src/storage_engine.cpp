#include "storage_engine.h"

StorageEngine::StorageEngine(const std::string &wal_path) : wal_(wal_path), memtable_() {
}

void StorageEngine::put(const std::string &key, const std::string &value) {
    bool result = memtable_.put(key, value);
    if (result)
        wal_.append(Operation::PUT, key, value);
}

void StorageEngine::del(const std::string &key) {
    bool result = memtable_.del(key);
    if (result)
        wal_.append(Operation::DELETE, key, "");
}

void StorageEngine::get(const std::string &key, std::string &out) const {
    memtable_.get(key, out);
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
    case Operation::DELETE:
        del(key);
        break;
    default:
        std::cerr << "Invalid command\n";
    }
}
