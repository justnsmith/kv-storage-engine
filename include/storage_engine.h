#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include "command_parser.h"
#include "memtable.h"
#include "sstable.h"
#include "types.h"
#include "wal.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

class WriteAheadLog;
class MemTable;

class StorageEngine {
  public:
    explicit StorageEngine(const std::string &wal_path);

    bool put(const std::string &key, const std::string &value);
    bool del(const std::string &key);
    bool get(const std::string &key, Entry &out) const;
    void ls() const;
    void flush();
    void clear();
    void handleCommand(const std::string &input);

    void recover();
    void clearData();

  private:
    WriteAheadLog wal_;
    MemTable memtable_;
    std::vector<SSTable> sstables_;
    uint64_t flush_counter_;
    uint64_t seq_number_;

    void checkFlush(bool debug = false);
    void checkCompaction();
};

#endif
