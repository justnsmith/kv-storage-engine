#include "follower.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

namespace distributed {

Follower::Follower(const ReplicationConfig &config) : config_(config) {
    std::cout << "[Follower] Initialized node " << config_.node_id << std::endl;
}

Follower::~Follower() {
    stop();
}

void Follower::start() {
    // Create listener socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("Failed to create listener socket");
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.replication_port);
    inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to bind replication port");
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to listen");
    }

    listener_thread_ = std::thread(&Follower::listenerLoop, this);
    apply_thread_ = std::thread(&Follower::applyLoop, this);

    std::cout << "[Follower] Listening on " << config_.host << ":" << config_.replication_port << std::endl;
}

void Follower::stop() {
    shutdown_ = true;
    apply_cv_.notify_all();

    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        close(listen_fd_);
        listen_fd_ = -1;
    }

    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }

    if (apply_thread_.joinable()) {
        apply_thread_.join();
    }

    std::cout << "[Follower] Stopped" << std::endl;
}

void Follower::listenerLoop() {
    while (!shutdown_) {
        struct sockaddr_in peer_addr{};
        socklen_t peer_len = sizeof(peer_addr);

        int peer_fd = accept(listen_fd_, (struct sockaddr *)&peer_addr, &peer_len);

        if (peer_fd < 0) {
            if (shutdown_)
                break;
            continue;
        }

        handleConnection(peer_fd);
        close(peer_fd);
    }
}

void Follower::handleConnection(int client_fd) {
    std::string buffer;
    char read_buf[4096];

    while (!shutdown_) {
        ssize_t n = recv(client_fd, read_buf, sizeof(read_buf), 0);

        if (n <= 0)
            break;

        buffer.append(read_buf, n);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (line.empty())
                continue;

            try {
                ReplicationMessage msg = ReplicationMessage::deserialize(line);
                handleReplication(msg);

                const char *ack = "+OK\n";
                send(client_fd, ack, strlen(ack), 0);

            } catch (const std::exception &e) {
                std::cerr << "[Follower] Failed to parse message: " << e.what() << std::endl;
            }
        }
    }
}

void Follower::handleReplication(const ReplicationMessage &msg) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    if (msg.term > current_term_) {
        current_term_ = msg.term;
    }

    for (const auto &entry : msg.entries) {
        log_.push_back(entry);
        std::cout << "[Follower] Received entry index=" << entry.index << std::endl;
    }

    if (msg.leader_commit > commit_index_) {
        commit_index_ = msg.leader_commit;
        apply_cv_.notify_one();
    }
}

void Follower::setApplyCallback(ApplyCallback cb) {
    apply_callback_ = cb;
}

void Follower::applyLoop() {
    std::cout << "[Follower] Apply loop started" << std::endl;

    while (!shutdown_) {
        std::unique_lock<std::mutex> lock(log_mutex_);

        std::cout << "[Follower] Apply loop waiting (commit=" << commit_index_ << ", applied=" << last_applied_ << ")" << std::endl;

        apply_cv_.wait(lock, [this] { return shutdown_ || commit_index_ > last_applied_; });

        if (shutdown_)
            break;

        std::cout << "[Follower] Woke up! commit=" << commit_index_ << ", applied=" << last_applied_ << std::endl;

        while (last_applied_ < commit_index_) {
            uint64_t next_index = last_applied_ + 1;

            std::cout << "[Follower] Looking for entry index=" << next_index << std::endl;

            auto it = std::find_if(log_.begin(), log_.end(), [next_index](const LogEntry &e) { return e.index == next_index; });

            if (it != log_.end()) {
                std::cout << "[Follower] Found entry index=" << next_index << std::endl;

                if (apply_callback_) {
                    lock.unlock();
                    apply_callback_(*it);
                    lock.lock();

                    last_applied_ = next_index;
                    std::cout << "[Follower] Applied entry index=" << next_index << std::endl;
                } else {
                    std::cerr << "[Follower] ERROR: No apply callback set!" << std::endl;
                    break;
                }
            } else {
                std::cerr << "[Follower] ERROR: Entry index=" << next_index << " not found in log!" << std::endl;
                std::cerr << "[Follower] Log size: " << log_.size() << std::endl;
                for (const auto &entry : log_) {
                    std::cerr << "[Follower]   - Entry index=" << entry.index << std::endl;
                }
                break;
            }
        }
    }

    std::cout << "[Follower] Apply loop exiting" << std::endl;
}

} // namespace distributed
