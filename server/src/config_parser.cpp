#include "config_parser.h"

std::optional<kv::ServerConfig> kv::ConfigParser::load(const std::string &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ConfigParser] Failed to open: " << filepath << std::endl;
        return std::nullopt;
    }

    ServerConfig config;
    std::unordered_map<std::string, std::string> values;
    std::vector<std::pair<std::string, uint16_t>> peers;

    std::string currentSection;
    std::string line;
    bool inPeersSection = false;
    bool inListItem = false;

    std::string peerHost;
    uint16_t peerPort = 0;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);

        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        size_t indent = 0;
        while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t')) {
            indent++;
        }

        bool startsWithDash = (trimmed[0] == '-');

        size_t colonPos = trimmed.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string key, value;

        if (startsWithDash) {
            // New list item
            std::string withoutDash = trim(trimmed.substr(1));
            colonPos = withoutDash.find(':');
            key = trim(withoutDash.substr(0, colonPos));
            value = trim(withoutDash.substr(colonPos + 1));
            inListItem = true;
        } else {
            key = trim(trimmed.substr(0, colonPos));
            value = trim(trimmed.substr(colonPos + 1));
        }

        // Remove quotes
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (!startsWithDash && value.empty() && indent == 0) {
            // Save pending peer before changing sections
            if (inPeersSection && !peerHost.empty() && peerPort != 0) {
                peers.push_back({peerHost, peerPort});
                peerHost.clear();
                peerPort = 0;
            }

            currentSection = key;
            inPeersSection = (key == "peers");
            inListItem = false;
            continue;
        }

        // Handle peer fields (either starts with dash OR is indented within a list item)
        if (inPeersSection && (startsWithDash || (inListItem && indent > 2))) {
            if (key == "host") {
                // Save previous peer if complete
                if (!peerHost.empty() && peerPort != 0) {
                    peers.push_back({peerHost, peerPort});
                }
                peerHost = value;
                peerPort = 0;
            } else if (key == "port") {
                peerPort = static_cast<uint16_t>(std::stoi(value));
                // Complete the peer
                if (!peerHost.empty()) {
                    peers.push_back({peerHost, peerPort});
                    peerHost.clear();
                    peerPort = 0;
                }
            }
            continue;
        }

        // Regular key-value pairs
        if (!currentSection.empty() && !inPeersSection) {
            std::string fullKey = currentSection + "." + key;
            values[fullKey] = value;
        }

        // Reset list item flag if we hit a new section-level key
        if (indent == 0 && !startsWithDash) {
            inListItem = false;
        }
    }

    // Save final peer
    if (!peerHost.empty() && peerPort != 0) {
        peers.push_back({peerHost, peerPort});
    }

    // Parse config values
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
    if (auto it = values.find("node.id"); it != values.end()) {
        config.node_id = static_cast<uint32_t>(std::stoi(it->second));
    }
    if (auto it = values.find("node.role"); it != values.end()) {
        config.role = it->second;
    }
    if (auto it = values.find("replication.port"); it != values.end()) {
        config.replication_port = static_cast<uint16_t>(std::stoi(it->second));
    }

    config.peers = peers;

    std::cout << "[ConfigParser] Loaded node " << config.node_id << " (" << config.role << ") with " << peers.size() << " peers"
              << std::endl;
    for (const auto &[host, port] : peers) {
        std::cout << "[ConfigParser]   Peer: " << host << ":" << port << std::endl;
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
