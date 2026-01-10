#include "heartbeat.h"
#include <chrono>
#include <iostream>

namespace distributed {

Heartbeat::Heartbeat(int interval_ms) : interval_ms_(interval_ms) {
}

Heartbeat::~Heartbeat() {
    stop();
}

void Heartbeat::start() {
    heartbeat_thread_ = std::thread(&Heartbeat::heartbeatLoop, this);
    std::cout << "[Heartbeat] Started (interval: " << interval_ms_ << "ms)" << std::endl;
}

void Heartbeat::stop() {
    shutdown_ = true;

    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }

    std::cout << "[Heartbeat] Stopped" << std::endl;
}

// cppcheck-suppress unusedFunction
void Heartbeat::setSendCallback(SendCallback cb) {
    send_callback_ = cb;
}

void Heartbeat::heartbeatLoop() {
    [[maybe_unused]] int retry_count = 0;

    while (!shutdown_) {
        if (send_callback_) {
            send_callback_();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
        retry_count++;
    }
}

} // namespace distributed
