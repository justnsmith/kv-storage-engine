#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <string>

enum class Operation : uint8_t { GET = 0, PUT = 1, DELETE = 2, LS = 3, FLUSH = 4, CLEAR = 5, ERROR = 6 };

enum class EntryType : uint8_t { PUT = 0, DELETE = 1 };

struct Entry {
    std::string value;
    uint64_t seq;
    EntryType type;
};

struct SSTableEntry {
    std::string key;
    std::string value;
    uint64_t seq;
    EntryType type;
};

#endif
