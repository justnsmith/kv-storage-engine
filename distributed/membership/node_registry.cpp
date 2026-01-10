#include "node_registry.h"

namespace distributed {

// cppcheck-suppress unusedFunction
void NodeRegistry::addPeer(const PeerInfo &peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_.push_back(peer);
}

// cppcheck-suppress unusedFunction
std::vector<PeerInfo> NodeRegistry::getPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_;
}

// cppcheck-suppress unusedFunction
void NodeRegistry::updatePeerConnection(const std::string &host, uint16_t port, bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(peers_.begin(), peers_.end(), [&](const PeerInfo &peer) { return peer.host == host && peer.port == port; });

    if (it != peers_.end()) {
        it->connected = connected;
    }
}

} // namespace distributed
