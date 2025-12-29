#ifndef SSTABLE_H
#define SSTABLE_H

#include "types.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

class SSTable {
  public:
    class Iterator {
      public:
        explicit Iterator(const SSTable &table);

        bool valid() const;

        const SSTableEntry &entry() const;

        void next();

      private:
        std::ifstream file_;
        uint64_t data_end_;
        bool valid_ = false;

        SSTableEntry current_;

        void readNext();
    };
    // Constructor for loading an existing SSTable from disk
    explicit SSTable(const std::string &path);

    // Constructor for creating a new SSTable from a MemTable snapshot
    // flush_counter is used to generate the filename
    static SSTable flush(const std::map<std::string, Entry> &snapshot, const std::string &dir_path, uint64_t flush_counter);

    // Retrieve a value by key. Returns std::nullopt if key not found.
    std::optional<std::string> get(const std::string &key) const;

    // Return the filename for this SSTable
    const std::string &filename() const;

    std::map<std::string, Entry> getData() const;

  private:
    std::string path_;
    std::string min_key_;
    std::string max_key_;
    uint64_t metadata_offset_;

    void loadMetadata();

    friend class Iterator;
};

#endif
