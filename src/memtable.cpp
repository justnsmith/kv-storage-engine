#include "memtable.h"

bool MemTable::put(const std::string &key, const std::string &value, uint64_t seqNumber) {
    memtable[key] = Entry{value, seqNumber, EntryType::PUT};
    return true;
}

bool MemTable::del(const std::string &key, uint64_t seqNumber) {
    bool existed = memtable.contains(key) && memtable.at(key).type != EntryType::DELETE;
    memtable[key] = Entry{"", seqNumber, EntryType::DELETE};
    return existed;
}

bool MemTable::get(const std::string &key, Entry &out) const {
    auto it = memtable.find(key);
    if (it == memtable.end()) {
        return false;
    }
    out = it->second;
    return true;
}

const std::map<std::string, Entry> &MemTable::snapshot() const {
    return memtable;
}

void MemTable::clear() {
    memtable.clear();
}

size_t MemTable::getSize() {
    size_t total = 0;

    constexpr size_t checksumSize = 4;
    constexpr size_t keyLenSize = 2;
    constexpr size_t valueLenSize = 2;
    constexpr size_t opSize = 1;
    constexpr size_t seqSize = sizeof(uint64_t);

    for (const auto &[key, entry] : memtable) {
        total += checksumSize + keyLenSize + valueLenSize + opSize + seqSize + key.size();

        if (entry.type == EntryType::PUT) {
            total += entry.value.size();
        }
    }

    return total;
}
