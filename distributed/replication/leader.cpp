#include "leader.h"
#include <iostream>

namespace distributed {

Leader::Leader(const ReplicationConfig &config) : config_(config), shipper_(std::make_unique<LogShipper>(config_.peers)) {
    std::cout << "[Leader] Initialized node " << config_.node_id << std::endl;
}

Leader::~Leader() {
    stop();
}

void Leader::start() {
    shipper_->start();
    apply_thread_ = std::thread(&Leader::applyLoop, this);

    // Start a heartbeat/connection retry thread
    std::thread retry_thread([this]() {
        int retry_count = 0;
        while (!shutdown_) {
            // Try to connect every second
            if (retry_count % 10 == 0) {
                shipper_->connectToPeers();
            }
            retry_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    retry_thread.detach();

    std::cout << "[Leader] Started" << std::endl;
}

void Leader::stop() {
    shutdown_ = true;
    apply_cv_.notify_all();

    if (apply_thread_.joinable()) {
        apply_thread_.join();
    }

    shipper_->stop();

    std::cout << "[Leader] Stopped" << std::endl;
}

bool Leader::replicate(const LogEntry &entry) {
    appendLog(entry);

    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        commit_index_ = log_.back().index;
    }

    ReplicationMessage msg;
    msg.term = current_term_;
    msg.leader_commit = commit_index_;
    msg.entries.push_back(log_.back());

    int ack_count = shipper_->shipEntries(msg);

    // Trigger local apply
    apply_cv_.notify_one();

    return ack_count > 0 || config_.peers.empty();
}

void Leader::appendLog(const LogEntry &entry) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    LogEntry indexed_entry = entry;
    indexed_entry.index = log_.empty() ? 1 : log_.back().index + 1;
    indexed_entry.term = current_term_;

    log_.push_back(indexed_entry);

    std::cout << "[Leader] Appended entry index=" << indexed_entry.index << std::endl;
}

void Leader::setApplyCallback(ApplyCallback cb) {
    apply_callback_ = cb;
}

void Leader::applyLoop() {
    while (!shutdown_) {
        std::unique_lock<std::mutex> lock(log_mutex_);

        apply_cv_.wait(lock, [this] { return shutdown_ || commit_index_ > last_applied_; });

        if (shutdown_)
            break;

        while (last_applied_ < commit_index_) {
            uint64_t next_index = last_applied_ + 1;

            auto it = std::find_if(log_.begin(), log_.end(), [next_index](const LogEntry &e) { return e.index == next_index; });

            if (it != log_.end() && apply_callback_) {
                lock.unlock();
                apply_callback_(*it);
                lock.lock();

                last_applied_ = next_index;
                std::cout << "[Leader] Applied entry index=" << next_index << std::endl;
            } else {
                break;
            }
        }
    }
}

} // namespace distributed
