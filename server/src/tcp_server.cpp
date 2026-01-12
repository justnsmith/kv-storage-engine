#include "tcp_server.h"
#include "connection_handler.h"
#include "follower.h"
#include "leader.h"
#include "replication_types.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace kv {

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_.load()) {
            std::cerr << "[ThreadPool] Cannot submit task, pool is shutdown" << std::endl;
            return;
        }
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_.exchange(true)) {
            return;
        }
    }
    cv_.notify_all();

    for (auto &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// cppcheck-suppress unusedFunction
size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return shutdown_.load() || !tasks_.empty(); });

            if (shutdown_.load() && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task) {
            try {
                task();
            } catch (const std::exception &e) {
                std::cerr << "[ThreadPool] Task exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[ThreadPool] Unknown task exception" << std::endl;
            }
        }
    }
}

TcpServer::TcpServer(const ServerConfig &config) : config_(config), thread_pool_(std::make_unique<ThreadPool>(config.num_threads)) {

    // Initialize storage engine
    engine_ = std::make_unique<StorageEngine>(config_.data_dir, config_.cache_size);
    std::cout << "[Server] Storage engine initialized with data directory: " << config_.data_dir << std::endl;

    // Initialize distributed layer if configured
    if (config_.node_id > 0 && !config_.role.empty()) {
        distributed::ReplicationConfig repl_config;
        repl_config.node_id = config_.node_id;
        repl_config.host = config_.host;
        repl_config.replication_port = config_.replication_port;

        // Build peer list
        for (const auto &[host, port] : config_.peers) {
            distributed::PeerInfo peer;
            peer.host = host;
            peer.port = port;
            peer.socket_fd = -1;
            peer.connected = false;
            repl_config.peers.push_back(peer);
            std::cout << "[Server] Added peer: " << host << ":" << port << std::endl; // ADD THIS
        }

        if (config_.role == "leader") {
            repl_config.role = distributed::NodeRole::LEADER;
            leader_ = std::make_unique<distributed::Leader>(repl_config);

            leader_->setApplyCallback([this](const distributed::LogEntry &entry) { applyLogEntry(entry); });

            std::cout << "[Server] Initialized as LEADER" << std::endl;

        } else if (config_.role == "follower") {
            repl_config.role = distributed::NodeRole::FOLLOWER;
            follower_ = std::make_unique<distributed::Follower>(repl_config);

            follower_->setApplyCallback([this](const distributed::LogEntry &entry) { applyLogEntry(entry); });

            std::cout << "[Server] Initialized as FOLLOWER" << std::endl;
        }
    } else {
        std::cout << "[Server] Running in standalone mode" << std::endl;
    }
}

TcpServer::~TcpServer() {
    shutdown();
}

void TcpServer::setSocketTimeout(int fd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// cppcheck-suppress unusedFunction
StorageEngine &TcpServer::engine() {
    return *engine_;
}

// cppcheck-suppress unusedFunction
size_t TcpServer::activeConnections() const {
    return active_connections_.load();
}

void TcpServer::run() {
    // Create client socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }

    setSocketTimeout(server_fd_, config_.accept_timeout_ms);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);

    if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
        close(server_fd_);
        throw std::runtime_error("Invalid address: " + config_.host);
    }

    if (bind(server_fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to bind: " + std::string(strerror(errno)));
    }

    if (listen(server_fd_, SOMAXCONN) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to listen: " + std::string(strerror(errno)));
    }

    running_.store(true);

    std::cout << "[Server] Listening on " << config_.host << ":" << config_.port << std::endl;

    // Start distributed layer
    if (leader_) {
        leader_->start();
    }
    if (follower_) {
        follower_->start();
    }

    std::cout << "[Server] Ready to accept connections" << std::endl;

    acceptLoop();
}

void TcpServer::shutdown() {
    if (!running_.exchange(false)) {
        return;
    }

    std::cout << "[Server] Shutting down..." << std::endl;

    // Stop distributed layer
    if (leader_) {
        leader_->stop();
    }
    if (follower_) {
        follower_->stop();
    }

    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }

    if (thread_pool_) {
        thread_pool_->shutdown();
    }

    std::cout << "[Server] Shutdown complete" << std::endl;
}

void TcpServer::acceptLoop() {
    while (running_.load()) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);

        if (client_fd < 0) {
            if (!running_.load())
                break;
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                continue;
            std::cerr << "[Server] Accept failed: " << strerror(errno) << std::endl;
            continue;
        }

        if (active_connections_.load() >= config_.max_connections) {
            std::cerr << "[Server] Connection limit reached" << std::endl;
            close(client_fd);
            continue;
        }

        setSocketTimeout(client_fd, 30000);
        active_connections_++;

        thread_pool_->submit([this, client_fd]() {
            try {
                handleClient(client_fd);
            } catch (const std::exception &e) {
                std::cerr << "[Server] Client handler exception: " << e.what() << std::endl;
            }
            close(client_fd);
            active_connections_--;
        });
    }
}

void TcpServer::handleClient(int client_fd) {
    auto executor = [this](const Request &req) { return executeCommand(req); };
    ConnectionHandler handler(client_fd, executor);
    handler.run();
}

Response TcpServer::executeCommand(const Request &req) {
    try {
        if (follower_ && (req.type == CommandType::PUT || req.type == CommandType::DELETE)) {
            return Response::error("NOT_LEADER - Write to leader at port 9000");
        }
        // If we're a leader, replicate writes
        if (leader_) {
            if (req.type == CommandType::PUT || req.type == CommandType::DELETE) {
                distributed::LogEntry entry;
                entry.term = 0;  // Will be set by leader
                entry.index = 0; // Will be set by leader
                entry.op = (req.type == CommandType::PUT) ? distributed::ReplicationOp::PUT : distributed::ReplicationOp::DELETE;
                entry.key = req.key;
                entry.value = req.value;

                if (!leader_->replicate(entry)) {
                    std::cerr << "[Server] Warning: Replication failed" << std::endl;
                }
            }
        }

        // Execute locally
        switch (req.type) {
        case CommandType::PUT: {
            bool success = engine_->put(req.key, req.value);
            return success ? Response::ok("STORED") : Response::error("STORE_FAILED");
        }

        case CommandType::GET: {
            Entry entry;
            bool found = engine_->get(req.key, entry);
            if (found && entry.type == EntryType::PUT) {
                return Response::okWithValue(entry.value);
            }
            return Response::notFound();
        }

        case CommandType::DELETE: {
            bool success = engine_->del(req.key);
            return success ? Response::ok("DELETED") : Response::error("DELETE_FAILED");
        }

        case CommandType::PING:
            return Response::ok("PONG");

        case CommandType::QUIT:
            return Response::ok("BYE");

        case CommandType::STATUS: {
            std::string status = "Node ID: " + std::to_string(config_.node_id) + "\n";

            if (leader_) {
                status += "Role: LEADER\n";
                status += "Term: " + std::to_string(leader_->getCurrentTerm()) + "\n";
                status += "Commit Index: " + std::to_string(leader_->getCommitIndex()) + "\n";
            } else if (follower_) {
                status += "Role: FOLLOWER\n";
                status += "Term: " + std::to_string(follower_->getCurrentTerm()) + "\n";
                status += "Commit Index: " + std::to_string(follower_->getCommitIndex()) + "\n";
            } else {
                status += "Role: STANDALONE\n";
            }

            status += "Active Connections: " + std::to_string(active_connections_.load());

            return Response::okWithValue(status);
        }

        default:
            return Response::error("UNKNOWN_COMMAND");
        }
    } catch (const std::exception &e) {
        std::cerr << "[Server] Command error: " << e.what() << std::endl;
        return Response::error("INTERNAL_ERROR");
    }
}

void TcpServer::applyLogEntry(const distributed::LogEntry &entry) {
    std::cout << "[Server] Applying entry index=" << entry.index << std::endl;

    try {
        switch (entry.op) {
        case distributed::ReplicationOp::PUT:
            engine_->put(entry.key, entry.value);
            break;

        case distributed::ReplicationOp::DELETE:
            engine_->del(entry.key);
            break;
        }
    } catch (const std::exception &e) {
        std::cerr << "[Server] Failed to apply: " << e.what() << std::endl;
    }
}

} // namespace kv
