#ifndef DISTRIBUTED_REPLICATION_TYPES_H
#define DISTRIBUTED_REPLICATION_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace distributed {

enum class NodeRole : uint8_t { LEADER = 0, FOLLOWER = 1 };

enum class ReplicationOp : uint8_t { PUT = 1, DELETE = 2 };

struct LogEntry {
    uint64_t term;
    uint64_t index;
    ReplicationOp op;
    std::string key;
    std::string value;

    std::string serialize() const;
    static LogEntry deserialize(const std::string &data);
};

struct ReplicationMessage {
    uint64_t term;
    uint64_t leader_commit;
    std::vector<LogEntry> entries;

    std::string serialize() const;
    static ReplicationMessage deserialize(const std::string &data);
};

struct PeerInfo {
    std::string host;
    uint16_t port;
    int socket_fd = -1;
    bool connected = false;
};

struct ReplicationConfig {
    uint32_t node_id;
    NodeRole role;
    std::string host;
    uint16_t replication_port;
    std::vector<PeerInfo> peers;
};

} // namespace distributed

#endif
