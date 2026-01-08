#include "tcp_server.h"
#include "connection_handler.h"

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
            return; // Already shutdown
        }
    }
    cv_.notify_all();

    for (auto &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

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

    // Initialize the storage engine
    std::string wal_path = config_.data_dir + "/wal.log";
    engine_ = std::make_unique<StorageEngine>(wal_path, config_.cache_size);

    std::cout << "[Server] Storage engine initialized" << std::endl;
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
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }

    // Allow address reuse
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }

    // Set socket timeout for graceful shutdown
    setSocketTimeout(server_fd_, config_.accept_timeout_ms);

    // Bind
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

    // Listen
    if (listen(server_fd_, SOMAXCONN) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to listen: " + std::string(strerror(errno)));
    }

    running_.store(true);
    std::cout << "[Server] Listening on " << config_.host << ":" << config_.port << std::endl;
    std::cout << "[Server] Thread pool size: " << config_.num_threads << std::endl;
    std::cout << "[Server] Max connections: " << config_.max_connections << std::endl;
    std::cout << "[Server] Ready to accept connections" << std::endl;

    acceptLoop();
}

void TcpServer::shutdown() {
    if (!running_.exchange(false)) {
        return; // Already shutdown
    }

    std::cout << "[Server] Shutting down..." << std::endl;

    // Close server socket to unblock accept()
    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }

    // Shutdown thread pool
    if (thread_pool_) {
        std::cout << "[Server] Waiting for " << thread_pool_->queueSize() << " pending tasks..." << std::endl;
        thread_pool_->shutdown();
    }

    // Wait for active connections to finish (with timeout)
    int wait_count = 0;
    while (active_connections_.load() > 0 && wait_count < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    if (active_connections_.load() > 0) {
        std::cout << "[Server] " << active_connections_.load() << " connections still active after timeout" << std::endl;
    }

    std::cout << "[Server] Shutdown complete" << std::endl;
}

void TcpServer::acceptLoop() {
    while (running_.load()) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);

        if (client_fd < 0) {
            if (!running_.load()) {
                // Shutdown in progress
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // Timeout or interrupted, continue
                continue;
            }
            std::cerr << "[Server] Accept failed: " << strerror(errno) << std::endl;
            continue;
        }

        // Check connection limit
        if (active_connections_.load() >= config_.max_connections) {
            std::cerr << "[Server] Connection limit reached, rejecting connection" << std::endl;
            close(client_fd);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "[Server] New connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

        // Set timeouts on client socket
        setSocketTimeout(client_fd, 30000); // 30 second timeout

        active_connections_++;

        // Handle client in thread pool
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

        default:
            return Response::error("UNKNOWN_COMMAND");
        }
    } catch (const std::exception &e) {
        std::cerr << "[Server] Command execution error: " << e.what() << std::endl;
        return Response::error("INTERNAL_ERROR");
    }
}

} // namespace kv
