#ifndef MEMTABLE_H
#define MEMTABLE_H

#include <iostream>
#include <map>
#include <string>

class MemTable {
  public:
    bool put(const std::string &key, const std::string &value);
    bool del(const std::string &key);
    bool get(const std::string &key, std::string &out) const;

  private:
    std::map<std::string, std::string> memtable;
};

#endif
