#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include "command_parser.h"
#include "lru_cache.h"
#include "memtable.h"
#include "sstable.h"
#include "types.h"
#include "wal.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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
    explicit StorageEngine(const std::string &wal_path, size_t cache_size = 1000);
    ~StorageEngine();
    StorageEngine(const StorageEngine &) = delete;
    StorageEngine &operator=(const StorageEngine &) = delete;

    bool put(const std::string &key, const std::string &value);
    bool del(const std::string &key);
    bool get(const std::string &key, Entry &out) const;
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
    WriteAheadLog wal_;
    MemTable memtable_;
    std::vector<SSTable> sstables_;
    std::vector<std::vector<SSTableMeta>> levels_;
    uint64_t flush_counter_;
    uint64_t seq_number_;
    mutable std::optional<LRUCache> cache_;

    // Threading components
    mutable std::shared_mutex state_mutex_;
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

    // Compaction methods
    void compactL0toL1();
    void compactlevelN(uint32_t level);

    // Background compaction coordination
    void compactionThreadLoop();
    void scheduleCompaction();
    bool shouldCompactUnlocked(uint32_t level) const;
    void maybeCompactBackground();
};

#endif
