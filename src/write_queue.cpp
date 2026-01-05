#include "write_queue.h"

WriteQueue::WriteQueue(size_t max_size) : max_size_(max_size) {
}

WriteQueue::~WriteQueue() {
    shutdown();
}

std::future<bool> WriteQueue::push(Operation op, const std::string &key, const std::string &value) {
    auto request = std::make_unique<WriteRequest>(op, key, value);
    std::future<bool> future = request->completion.get_future();

    {
        std::unique_lock<std::mutex> lock(mutex_);

        not_full_cv_.wait(lock, [this] { return queue_.size() < max_size_ || shutdown_; });

        if (shutdown_) {
            request->completion.set_value(false);
            return future;
        }

        queue_.push(std::move(request));
    }

    not_empty_cv_.notify_one();
    return future;
}

std::optional<std::unique_ptr<WriteRequest>> WriteQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);

    not_empty_cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });

    if (queue_.empty()) {
        return std::nullopt;
    }

    auto request = std::move(queue_.front());
    queue_.pop();

    not_full_cv_.notify_one();

    return request;
}

void WriteQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    not_empty_cv_.notify_all();
    not_full_cv_.notify_all();
}

bool WriteQueue::isShutdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}

size_t WriteQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}
