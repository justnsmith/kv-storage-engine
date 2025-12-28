#ifndef WAL_H
#define WAL_H

#include "types.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <zlib.h>

class WriteAheadLog {
  public:
    explicit WriteAheadLog(const std::string &path);
    void append(Operation op, const std::string &key, const std::string &value, uint64_t seqNumber);
    void replay(std::function<void(uint64_t, Operation, std::string &, std::string &)> apply);
    bool empty() const;

  private:
    const std::string path_;
    static uint32_t calculateChecksum(Operation op, const std::string &key, const std::string &value, const uint64_t seqNumber);
};
#endif
