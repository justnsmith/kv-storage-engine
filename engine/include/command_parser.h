#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "types.h"
#include <cstddef>
#include <string>

Operation parseCommand(const std::string &input, std::string &key, std::string &value);
#endif
