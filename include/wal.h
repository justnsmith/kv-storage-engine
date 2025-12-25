#ifndef WAL_H
#define WAL_H

#include "types.h"
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
    void append(Operation op, const std::string &key, const std::string &value);
    void replay(std::function<void(Operation, std::string &, std::string &)> apply);

  private:
    const std::string path_;
    static uint32_t calculateChecksum(Operation op, const std::string &key, const std::string &value);
};
#endif;
