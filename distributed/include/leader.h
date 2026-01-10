#ifndef DISTRIBUTED_LEADER_H
#define DISTRIBUTED_LEADER_H

#include "log_shipper.h"
#include "replication_types.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace distributed {

class Leader {
  public:
    using ApplyCallback = std::function<void(const LogEntry &)>;

    explicit Leader(const ReplicationConfig &config);
    ~Leader();

    void start();
    void stop();

    bool replicate(const LogEntry &entry);
    void setApplyCallback(ApplyCallback cb);

    uint64_t getCurrentTerm() const {
        return current_term_;
    }
    uint64_t getCommitIndex() const {
        return commit_index_;
    }

  private:
    ReplicationConfig config_;
    std::unique_ptr<LogShipper> shipper_;

    std::vector<LogEntry> log_;
    std::atomic<uint64_t> current_term_{0};
    std::atomic<uint64_t> commit_index_{0};
    std::atomic<uint64_t> last_applied_{0};

    mutable std::mutex log_mutex_;
    ApplyCallback apply_callback_;

    // Background threads
    std::thread apply_thread_;
    std::atomic<bool> shutdown_{false};
    std::condition_variable apply_cv_;

    void appendLog(const LogEntry &entry);
    void applyLoop();
};

} // namespace distributed

#endif
