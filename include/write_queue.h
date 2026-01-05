#ifndef WRITE_QUEUE_H
#define WRITE_QUEUE_H

#include "types.h"
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

struct WriteRequest {
    Operation op;
    std::string key;
    std::string value;
    std::promise<bool> completion;

    WriteRequest(Operation op_, std::string key_, std::string value_) : op(op_), key(std::move(key_)), value(std::move(value_)) {
    }

    WriteRequest(WriteRequest &&) = default;
    WriteRequest &operator=(WriteRequest &&) = default;
    WriteRequest(const WriteRequest &) = delete;
    WriteRequest &operator=(const WriteRequest &) = delete;
};

class WriteQueue {
  public:
    explicit WriteQueue(size_t max_size = 10000);
    ~WriteQueue();

    std::future<bool> push(Operation op, const std::string &key, const std::string &value);
    std::optional<std::unique_ptr<WriteRequest>> pop();
    void shutdown();
    bool isShutdown() const;
    size_t size() const;

  private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    std::condition_variable not_full_cv_;
    std::queue<std::unique_ptr<WriteRequest>> queue_;
    size_t max_size_;
    bool shutdown_{false};
};

#endif
