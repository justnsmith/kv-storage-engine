#include "memtable.h"

bool MemTable::put(const std::string &key, const std::string &value) {
    if (!memtable.contains(key)) {
        memtable[key] = value;
        return true;
    } else {
        std::cerr << "Key already exists\n";
        return false;
    }
}

bool MemTable::del(const std::string &key) {
    if (memtable.contains(key)) {
        memtable.erase(key);
        return true;
    } else {
        std::cerr << "Key does not exist\n";
        return false;
    }
}

bool MemTable::get(const std::string &key, std::string &out) const {
    if (memtable.contains(key)) {
        out = memtable.at(key);
        return true;
    } else {
        std::cerr << "Key does not exist\n";
        return false;
    }
}
