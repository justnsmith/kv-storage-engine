#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include "command_parser.h"
#include "memtable.h"
#include "sstable.h"
#include "types.h"
#include "wal.h"
#include <string>

class WriteAheadLog;
class MemTable;

class StorageEngine {
  public:
    explicit StorageEngine(const std::string &wal_path);

    void put(const std::string &key, const std::string &value);
    void del(const std::string &key);
    void get(const std::string &key, std::string &out) const;
    void handleCommand(const std::string &input);

    void recover();

  private:
    WriteAheadLog wal_;
    MemTable memtable_;
    std::vector<SSTable> sstables_;
    uint64_t flush_counter_;

    void checkFlush();
};

#endif
