#ifndef DISTRIBUTED_HEARTBEAT_H
#define DISTRIBUTED_HEARTBEAT_H

#include "replication_types.h"
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace distributed {

class Heartbeat {
  public:
    using SendCallback = std::function<void()>;

    explicit Heartbeat(int interval_ms = 100);
    ~Heartbeat();

    void start();
    void stop();

    void setSendCallback(SendCallback cb);

  private:
    int interval_ms_;
    std::thread heartbeat_thread_;
    std::atomic<bool> shutdown_{false};
    SendCallback send_callback_;

    void heartbeatLoop();
};

} // namespace distributed

#endif
