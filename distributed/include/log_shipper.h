#ifndef DISTRIBUTED_LOG_SHIPPER_H
#define DISTRIBUTED_LOG_SHIPPER_H

#include "replication_types.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

namespace distributed {

class LogShipper {
  public:
    explicit LogShipper(std::vector<PeerInfo> &peers);
    ~LogShipper();

    static void start();
    void stop();

    // Ship entries to followers, returns number of successful acks
    int shipEntries(const ReplicationMessage &msg);

    // Attempt to connect/reconnect to peers
    void connectToPeers();

  private:
    std::vector<PeerInfo> &peers_;
    mutable std::mutex peers_mutex_;
    std::atomic<bool> shutdown_{false};

    static bool sendToPeer(PeerInfo &peer, const std::string &data);
};

} // namespace distributed

#endif
