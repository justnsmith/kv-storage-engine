#include "config_parser.h"

std::optional<kv::ServerConfig> kv::ConfigParser::load(const std::string &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return std::nullopt;
    }

    ServerConfig config;
    std::unordered_map<std::string, std::string> values;
    std::string currentSection;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        bool isIndented = !line.empty() && (line[0] == ' ' || line[0] == '\t');

        size_t colonPos = trimmed.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, colonPos));
        std::string value = trim(trimmed.substr(colonPos + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (!isIndented) {
            if (value.empty()) {
                currentSection = key;
            } else {
                values[key] = value;
                currentSection.clear();
            }
        } else {
            std::string fullKey = currentSection.empty() ? key : currentSection + "." + key;
            values[fullKey] = value;
        }
    }

    if (auto it = values.find("server.host"); it != values.end()) {
        config.host = it->second;
    }
    if (auto it = values.find("server.port"); it != values.end()) {
        config.port = static_cast<uint16_t>(std::stoi(it->second));
    }
    if (auto it = values.find("server.threads"); it != values.end()) {
        config.num_threads = static_cast<size_t>(std::stoi(it->second));
    }
    if (auto it = values.find("storage.data_dir"); it != values.end()) {
        config.data_dir = it->second;
    }
    if (auto it = values.find("storage.cache_size"); it != values.end()) {
        config.cache_size = static_cast<size_t>(std::stoi(it->second));
    }

    return config;
}

std::string kv::ConfigParser::trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
