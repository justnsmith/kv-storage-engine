#ifndef MEMTABLE_H
#define MEMTABLE_H

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <cstdint>

class MemTable {
  public:
    bool put(const std::string &key, const std::string &value);
    bool del(const std::string &key);
    bool get(const std::string &key, std::string &out) const;
    const std::map<std::string, std::string>& snapshot() const;
    void clear();
    size_t getSize();

  private:
    std::map<std::string, std::string> memtable;
};

#endif
