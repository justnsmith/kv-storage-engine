#ifndef DISTRIBUTED_NODE_REGISTRY_H
#define DISTRIBUTED_NODE_REGISTRY_H

#include "replication_types.h"
#include <mutex>
#include <vector>

namespace distributed {

class NodeRegistry {
  public:
    NodeRegistry() = default;

    void addPeer(const PeerInfo &peer);
    std::vector<PeerInfo> getPeers() const;
    void updatePeerConnection(const std::string &host, uint16_t port, bool connected);

  private:
    std::vector<PeerInfo> peers_;
    mutable std::mutex mutex_;
};

} // namespace distributed

#endif
