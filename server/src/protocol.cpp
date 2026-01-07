#include "protocol.h"

kv::Response ok(const std::string&msg) {
    return {true, msg, std::nullopt};
}

kv::Response okWithValue(const std::string&value) {
    return {true, "OK", value};
}

kv::Response notFound() {
    return {false, "NOT_FOUND", std::nullopt};
}

kv::Response error(const std::string&msg) {
    return {false, msg, std::nullopt};
}

std::string kv::Response::serialize() const {
    if (value.has_value()) {
        return "+VALUE " + *value + "\r\n";
    }
    if (success) {
        return "+OK " + message + "\r\n";
    }
    return "-ERR " + message + "\r\n";
}
