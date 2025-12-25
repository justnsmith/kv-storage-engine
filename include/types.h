#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

enum class Operation : uint8_t { GET = 0, PUT = 1, DELETE = 2, ERROR = 3 };

#endif
