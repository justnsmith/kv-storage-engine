#include "replication_types.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace distributed {

std::string LogEntry::serialize() const {
    std::ostringstream oss;
    oss << term << "|" << index << "|" << static_cast<int>(op) << "|" << key.size() << "|" << key << "|" << value.size() << "|" << value;
    return oss.str();
}

LogEntry LogEntry::deserialize(const std::string &data) {
    LogEntry entry;
    size_t pos = 0;

    auto nextToken = [&]() -> std::string {
        size_t delim = data.find('|', pos);
        if (delim == std::string::npos) {
            throw std::runtime_error("Malformed log entry: delimiter not found at pos " + std::to_string(pos));
        }
        std::string token = data.substr(pos, delim - pos);
        pos = delim + 1;
        return token;
    };

    try {
        std::string termStr = nextToken();
        std::string indexStr = nextToken();
        std::string opStr = nextToken();

        entry.term = std::stoull(termStr);
        entry.index = std::stoull(indexStr);
        entry.op = static_cast<ReplicationOp>(std::stoi(opStr));

        size_t keyLen = std::stoull(nextToken());
        entry.key = data.substr(pos, keyLen);
        pos += keyLen + 1; // +1 for delimiter

        size_t valueLen = std::stoull(nextToken());
        entry.value = data.substr(pos, valueLen);

    } catch (const std::exception &e) {
        std::cerr << "[LogEntry] Deserialization failed: " << e.what() << std::endl;
        std::cerr << "[LogEntry] Data: '" << data << "'" << std::endl;
        std::cerr << "[LogEntry] Data length: " << data.length() << std::endl;
        throw;
    }

    return entry;
}

std::string ReplicationMessage::serialize() const {
    std::ostringstream oss;
    oss << term << "|" << leader_commit << "|" << entries.size();

    for (const auto &entry : entries) {
        std::string serialized = entry.serialize();
        oss << "|" << serialized.size() << "|" << serialized;
    }

    return oss.str();
}

ReplicationMessage ReplicationMessage::deserialize(const std::string &data) {
    ReplicationMessage msg;
    size_t pos = 0;

    auto nextToken = [&]() -> std::string {
        size_t delim = data.find('|', pos);
        if (delim == std::string::npos) {
            throw std::runtime_error("Malformed replication message: delimiter not found");
        }
        std::string token = data.substr(pos, delim - pos);
        pos = delim + 1;
        return token;
    };

    try {
        msg.term = std::stoull(nextToken());
        msg.leader_commit = std::stoull(nextToken());

        size_t numEntries = std::stoull(nextToken());

        for (size_t i = 0; i < numEntries; ++i) {
            size_t entryLen = std::stoull(nextToken());
            std::string entryData = data.substr(pos, entryLen);
            pos += entryLen + 1;
            msg.entries.push_back(LogEntry::deserialize(entryData));
        }

    } catch (const std::exception &e) {
        std::cerr << "[ReplicationMessage] Deserialization failed: " << e.what() << std::endl;
        std::cerr << "[ReplicationMessage] Data: '" << data << "'" << std::endl;
        throw;
    }

    return msg;
}

} // namespace distributed
