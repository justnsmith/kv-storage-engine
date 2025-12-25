#include "memtable.h"

bool MemTable::put(const std::string &key, const std::string &value) {
    memtable[key] = value;
    return true;
}

bool MemTable::del(const std::string &key) {
    if (memtable.contains(key)) {
        memtable.erase(key);
        return true;
    } else {
        return false;
    }
}

bool MemTable::get(const std::string &key, std::string &out) const {
    if (memtable.contains(key)) {
        out = memtable.at(key);
        return true;
    } else {
        return false;
    }
}

const std::map<std::string, std::string> &MemTable::snapshot() const {
    return memtable;
}

void MemTable::clear() {
    memtable.clear();
}

size_t MemTable::getSize() {
    size_t total = 0;
    const uint8_t checksumByteSize = 4;
    const uint8_t keyLenByteSize = 2;
    const uint8_t valueLenByteSize = 2;
    const uint8_t opByteSize = 1;
    for (const auto &[k, v] : memtable) {
        total += checksumByteSize + keyLenByteSize + valueLenByteSize + opByteSize + k.size() + v.size();
    }
    return total;
}
