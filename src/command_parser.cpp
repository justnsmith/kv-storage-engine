#include "command_parser.h"

// Parse input commands like: put("key", "value"), get("key"), delete("key")
Operation parseCommand(const std::string &input, std::string &key, std::string &value) {
    key.clear();
    value.clear();

    if (input.substr(0, 4) == "put(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);

        size_t startValue = input.find('"', endKey + 1);
        size_t endValue = input.find('"', startValue + 1);
        if (startValue == std::string::npos || endValue == std::string::npos)
            return Operation::ERROR;

        value = input.substr(startValue + 1, endValue - startValue - 1);
        return Operation::PUT;

    } else if (input.substr(0, 4) == "get(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);
        return Operation::GET;

    } else if (input.substr(0, 7) == "delete(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);
        return Operation::DELETE;
    } else if (input.substr(0, 2) == "ls") {
        if (input.size() != 2) {
            return Operation::ERROR;
        }
        return Operation::LS;
    } else if (input.substr(0, 5) == "flush") {
        if (input.size() != 5) {
            return Operation::ERROR;
        }
        return Operation::FLUSH;
    } else if (input.substr(0, 5) == "clear") {
        if (input.size() != 5) {
            return Operation::ERROR;
        }
        return Operation::CLEAR;
    }

    return Operation::ERROR;
}
