#ifndef WAL_H
#define WAL_H

#include "types.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <zlib.h>

class WriteAheadLog {
  public:
    explicit WriteAheadLog(const std::string &path, int sync_interval_ms = 10);
    ~WriteAheadLog();

    void append(Operation op, const std::string &key, const std::string &value, uint64_t seqNumber);
    void replay(std::function<void(uint64_t, Operation, std::string &, std::string &)> apply);
    bool empty() const;
    void flush();
    void syncFlush();

  private:
    std::string path_;
    int fd_{-1};

    std::vector<char> write_buffer_;
    std::vector<char> sync_buffer_;
    size_t write_buffer_size_ = 0;
    std::mutex buffer_mutex_;

    // Background sync thread
    std::thread sync_thread_;
    std::atomic<bool> shutdown_{false};
    std::condition_variable sync_cv_;
    std::mutex sync_mutex_;
    bool sync_requested_{false};

    // For blocking sync
    std::condition_variable sync_done_cv_;
    std::atomic<uint64_t> sync_generation_{0};
    std::atomic<uint64_t> synced_generation_{0};

    int sync_interval_ms_;
    static constexpr size_t MAX_BUFFER_SIZE = 256 * 1024; // 256KB buffer

    static uint32_t calculateChecksum(Operation op, const std::string &key, const std::string &value, uint64_t seqNumber);
    void syncThreadLoop();
    void doSync();
};

#endif
