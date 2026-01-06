#include "memtable.h"

bool MemTable::put(const std::string &key, const std::string &value, uint64_t seqNumber) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    memtable_[key] = Entry{value, seqNumber, EntryType::PUT};
    return true;
}

bool MemTable::del(const std::string &key, uint64_t seqNumber) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool existed = memtable_.contains(key) && memtable_.at(key).type != EntryType::DELETE;
    memtable_[key] = Entry{"", seqNumber, EntryType::DELETE};
    return existed;
}

bool MemTable::get(const std::string &key, Entry &out) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = memtable_.find(key);
    if (it == memtable_.end()) {
        return false;
    }
    out = it->second;
    return true;
}

const std::map<std::string, Entry> MemTable::snapshot() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return memtable_;
}

void MemTable::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    memtable_.clear();
}

size_t MemTable::getSize() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    size_t total = 0;
    constexpr size_t checksumSize = 4;
    constexpr size_t keyLenSize = 2;
    constexpr size_t valueLenSize = 2;
    constexpr size_t opSize = 1;
    constexpr size_t seqSize = sizeof(uint64_t);

    for (const auto &[key, entry] : memtable_) {
        total += checksumSize + keyLenSize + valueLenSize + opSize + seqSize + key.size();
        if (entry.type == EntryType::PUT) {
            total += entry.value.size();
        }
    }

    return total;
}
