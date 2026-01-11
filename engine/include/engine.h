#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include "command_parser.h"
#include "lru_cache.h"
#include "memtable.h"
#include "sstable.h"
#include "table_version.h"
#include "types.h"
#include "wal.h"
#include "write_queue.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

class WriteAheadLog;
class MemTable;

class StorageEngine {
  public:
    explicit StorageEngine(const std::string &data_dir, size_t cache_size = 1000);
    ~StorageEngine();
    StorageEngine(const StorageEngine &) = delete;
    StorageEngine &operator=(const StorageEngine &) = delete;

    bool put(const std::string &key, const std::string &value);
    bool del(const std::string &key);
    bool get(const std::string &key, Entry &out) const;

    std::future<bool> putAsync(const std::string &key, const std::string &value);
    std::future<bool> delAsync(const std::string &key);

    void ls() const;
    void flush();
    void handleCommand(const std::string &input);
    void recover();
    void clearData();

    void waitForCompaction();
    void pauseCompaction();
    void resumeCompaction();

  private:
    // Core storage components
    std::string data_dir_;
    WriteAheadLog wal_;
    MemTable memtable_;
    std::shared_ptr<MemTable> immutable_memtable_; // Immutable memtable being flushed (atomic access)
    VersionManager version_manager_;
    uint64_t flush_counter_;
    uint64_t seq_number_;
    mutable std::optional<LRUCache> cache_;

    // Threading components - protects flush_counter_, seq_number_, metadata writes
    mutable std::mutex metadata_mutex_;

    // Writer thread
    WriteQueue write_queue_;
    std::thread writer_thread_;
    std::atomic<bool> writer_shutdown_{false};

    // Flush thread
    std::thread flush_thread_;
    mutable std::mutex flush_mutex_;
    mutable std::condition_variable flush_cv_;
    std::atomic<bool> flush_pending_{false};

    // Compaction thread
    std::thread compaction_thread_;
    std::atomic<bool> shutdown_{false};
    std::condition_variable compaction_cv_;
    std::mutex compaction_mutex_;
    std::atomic<bool> compaction_needed_{false};
    std::atomic<bool> compaction_in_progress_{false};
    std::atomic<bool> compaction_paused_{false};

    // Core methods
    void checkFlush(bool debug = false);
    void loadLevelMetadata();
    void loadSSTables();
    void saveMetadata();

    void writerThreadLoop();
    void flushThreadLoop();
    void triggerFlush();

    // Compaction methods
    void compactL0toL1();
    void compactlevelN(uint32_t level);

    // Background compaction coordination
    void compactionThreadLoop();
    void scheduleCompaction();
    bool shouldCompactUnlocked(uint32_t level) const;
    static bool shouldCompactUnlocked(uint32_t level, const std::shared_ptr<TableVersion> &version);
    void maybeCompactBackground();
};

#endif
