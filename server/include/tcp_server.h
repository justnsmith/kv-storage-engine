#ifndef TCP_SERVER_H
#define TCP_SERVER_H
#include "engine.h"
#include "protocol.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Forward declarations from distributed layer
namespace distributed {
class Leader;
class Follower;
struct LogEntry;
} // namespace distributed

namespace kv {

class ThreadPool {
  public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();
    void submit(std::function<void()> task);
    void shutdown();
    size_t queueSize() const;

  private:
    void workerLoop();
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_{false};
};

struct ServerConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
    int num_threads = 4;
    std::string data_dir = "data";
    size_t cache_size = 1000;
    int accept_timeout_ms = 1000;
    size_t max_connections = 1000;

    // Replication config
    uint32_t node_id = 0;
    std::string role;
    std::vector<std::pair<std::string, uint16_t>> peers;
};

class TcpServer {
  public:
    explicit TcpServer(const ServerConfig &config);
    ~TcpServer();

    void run();
    void shutdown();
    StorageEngine &engine();
    size_t activeConnections() const;

  private:
    void acceptLoop();
    void handleClient(int client_fd);
    Response executeCommand(const Request &req);
    void applyLogEntry(const distributed::LogEntry &entry);
    static void setSocketTimeout(int fd, int timeout_ms);

    ServerConfig config_;
    std::unique_ptr<StorageEngine> engine_;
    std::unique_ptr<ThreadPool> thread_pool_;

    // Distributed replication
    std::unique_ptr<distributed::Leader> leader_;
    std::unique_ptr<distributed::Follower> follower_;

    int server_fd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<size_t> active_connections_{0};
};

} // namespace kv
#endif
