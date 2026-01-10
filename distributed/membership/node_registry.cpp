#include "node_registry.h"

namespace distributed {

void NodeRegistry::addPeer(const PeerInfo &peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_.push_back(peer);
}

std::vector<PeerInfo> NodeRegistry::getPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_;
}

void NodeRegistry::updatePeerConnection(const std::string &host, uint16_t port, bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &peer : peers_) {
        if (peer.host == host && peer.port == port) {
            peer.connected = connected;
            break;
        }
    }
}

} // namespace distributed
