#ifndef SSTABLE_H
#define SSTABLE_H

#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

class SSTable {
  public:
    // Constructor for loading an existing SSTable from disk
    explicit SSTable(const std::string &path);

    // Constructor for creating a new SSTable from a MemTable snapshot
    // flush_counter is used to generate the filename
    static SSTable flush(const std::map<std::string, std::string> &snapshot, const std::string &dir_path, uint64_t flush_counter);

    // Retrieve a value by key. Returns std::nullopt if key not found.
    std::optional<std::string> get(const std::string &key) const;

    // Return the filename for this SSTable
    std::string filename() const;

    std::map<std::string, std::string> getData() const;

  private:
    std::string path_;
    std::string min_key_;
    std::string max_key_;
};

#endif
