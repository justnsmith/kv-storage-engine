#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <string>

enum class Operation : uint8_t { GET = 0, PUT = 1, DELETE = 2, LS = 3, ERROR = 4 };

struct Entry {
    std::string value;
    uint64_t seq;
};

#endif
