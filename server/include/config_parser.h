#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "tcp_server.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace kv {

// Simple YAML-like config parser (handles basic key: value format)
class ConfigParser {
  public:
    static std::optional<ServerConfig> load(const std::string &filepath);

  private:
    static std::string trim(const std::string &s);
};

} // namespace kv

#endif
