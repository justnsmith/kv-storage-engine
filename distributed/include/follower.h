#ifndef DISTRIBUTED_FOLLOWER_H
#define DISTRIBUTED_FOLLOWER_H

#include "replication_types.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace distributed {

class Follower {
  public:
    using ApplyCallback = std::function<void(const LogEntry &)>;

    explicit Follower(const ReplicationConfig &config);
    ~Follower();

    void start();
    void stop();

    void handleReplication(const ReplicationMessage &msg);
    void setApplyCallback(ApplyCallback cb);

    uint64_t getCurrentTerm() const {
        return current_term_;
    }
    uint64_t getCommitIndex() const {
        return commit_index_;
    }

  private:
    ReplicationConfig config_;

    std::vector<LogEntry> log_;
    std::atomic<uint64_t> current_term_{0};
    std::atomic<uint64_t> commit_index_{0};
    std::atomic<uint64_t> last_applied_{0};

    mutable std::mutex log_mutex_;
    ApplyCallback apply_callback_;

    // Listener thread
    std::thread listener_thread_;
    std::atomic<bool> shutdown_{false};
    std::condition_variable apply_cv_;
    int listen_fd_ = -1;

    // Background threads
    std::thread apply_thread_;

    void listenerLoop();
    void handleConnection(int client_fd);
    void applyLoop();
};

} // namespace distributed

#endif
