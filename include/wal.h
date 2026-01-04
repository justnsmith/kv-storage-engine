#ifndef WAL_H
#define WAL_H

#include "types.h"
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>
#include <zlib.h>

class WriteAheadLog {
  public:
    explicit WriteAheadLog(const std::string &path);
    ~WriteAheadLog();

    void append(Operation op, const std::string &key, const std::string &value, uint64_t seqNumber);
    void replay(std::function<void(uint64_t, Operation, std::string &, std::string &)> apply);
    bool empty() const;

    void flush();

  private:
    std::string path_;
    std::ofstream log_file_;
    std::vector<char> buffer_;
    size_t buffer_size_ = 0;
    static constexpr size_t MAX_BUFFER_SIZE = 64 * 1024;

    static uint32_t calculateChecksum(Operation op, const std::string &key, const std::string &value, uint64_t seqNumber);
    void flushBuffer();
};

#endif
