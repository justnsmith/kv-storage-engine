#ifndef MEMTABLE_H
#define MEMTABLE_H

#include "types.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>

class MemTable {
  public:
    bool put(const std::string &key, const std::string &value, uint64_t seqNumber);
    bool del(const std::string &key);
    bool get(const std::string &key, std::string &out) const;
    const std::map<std::string, Entry> &snapshot() const;
    void clear();
    size_t getSize();

  private:
    std::map<std::string, Entry> memtable;
};

#endif
