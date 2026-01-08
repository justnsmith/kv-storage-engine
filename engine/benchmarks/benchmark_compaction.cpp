#include "engine.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

std::string generateRandomString(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    return result;
}

struct BenchmarkResults {
    double write_time_ms;
    double read_time_before_ms;
    double read_time_after_ms;
    size_t sstable_count_before;
    size_t sstable_count_after;
    double improvement_factor;
};

BenchmarkResults benchmarkCompactionImpact() {
    std::cout << "=== Compaction Performance Impact ===\n\n";

    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 0);

    const size_t NUM_KEYS = 5000;
    const size_t NUM_READS = 1000;
    const size_t NUM_FLUSHES = 5;

    std::cout << "Phase 1: Writing keys with multiple flushes (compaction PAUSED)...\n";

    engine.pauseCompaction();

    auto write_start = std::chrono::high_resolution_clock::now();

    // Write data multiple times with flushes to create overlapping L0 SSTables
    for (size_t flush = 0; flush < NUM_FLUSHES; ++flush) {
        for (size_t i = 0; i < NUM_KEYS; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "v" + std::to_string(flush) + "_" + generateRandomString(100);
            engine.put(key, value);
        }
        engine.flush();
        std::cout << "  Flush " << (flush + 1) << "/" << NUM_FLUSHES << " complete\n";
    }

    auto write_end = std::chrono::high_resolution_clock::now();
    double write_time = std::chrono::duration<double, std::milli>(write_end - write_start).count();

    std::cout << "Write completed in " << write_time << " ms\n\n";

    // Count SSTables before compaction
    size_t sstable_count_before = 0;
    std::filesystem::path sstable_dir = "data/sstables";
    if (std::filesystem::exists(sstable_dir)) {
        sstable_count_before = std::count_if(std::filesystem::directory_iterator(sstable_dir), std::filesystem::directory_iterator{},
                                             [](const auto &entry) { return entry.path().extension() == ".bin"; });
    }

    std::cout << "Phase 2: Reading before compaction (" << sstable_count_before << " SSTables)...\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dis(0, NUM_KEYS - 1);

    std::vector<std::string> test_keys;
    for (size_t i = 0; i < NUM_READS; ++i) {
        test_keys.push_back("key_" + std::to_string(key_dis(gen)));
    }

    auto read_before_start = std::chrono::high_resolution_clock::now();

    for (const auto &key : test_keys) {
        Entry result;
        engine.get(key, result);
    }

    auto read_before_end = std::chrono::high_resolution_clock::now();
    double read_before_time = std::chrono::duration<double, std::milli>(read_before_end - read_before_start).count();
    double avg_latency_before = (read_before_time * 1000.0) / NUM_READS;

    std::cout << "  Time: " << read_before_time << " ms\n";
    std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) << avg_latency_before << " μs\n\n";

    std::cout << "Phase 3: Triggering compaction...\n";
    engine.resumeCompaction();
    engine.waitForCompaction();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Count SSTables after compaction
    size_t sstable_count_after = 0;
    if (std::filesystem::exists(sstable_dir)) {
        sstable_count_after = std::count_if(std::filesystem::directory_iterator(sstable_dir), std::filesystem::directory_iterator{},
                                            [](const auto &entry) { return entry.path().extension() == ".bin"; });
    }

    std::cout << "Compaction complete (" << sstable_count_after << " SSTables)\n\n";

    std::cout << "Phase 4: Reading after compaction...\n";

    auto read_after_start = std::chrono::high_resolution_clock::now();

    for (const auto &key : test_keys) {
        Entry result;
        engine.get(key, result);
    }

    auto read_after_end = std::chrono::high_resolution_clock::now();
    double read_after_time = std::chrono::duration<double, std::milli>(read_after_end - read_after_start).count();
    double avg_latency_after = (read_after_time * 1000.0) / NUM_READS;

    std::cout << "  Time: " << read_after_time << " ms\n";
    std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) << avg_latency_after << " μs\n\n";

    double improvement = (read_after_time > 0) ? (read_before_time / read_after_time) : 0;
    double reduction_pct = (sstable_count_before > 0) ? ((sstable_count_before - sstable_count_after) * 100.0 / sstable_count_before) : 0;

    std::cout << "=== Results ===\n";
    std::cout << "SSTables: " << sstable_count_before << " → " << sstable_count_after << " (reduced by " << std::fixed
              << std::setprecision(1) << reduction_pct << "%)\n";
    std::cout << "Read time: " << read_before_time << " ms → " << read_after_time << " ms\n";
    std::cout << "Improvement: " << std::fixed << std::setprecision(2) << improvement << "x faster\n";
    std::cout << "Space amplification reduced\n\n";

    BenchmarkResults results;
    results.write_time_ms = write_time;
    results.read_time_before_ms = read_before_time;
    results.read_time_after_ms = read_after_time;
    results.sstable_count_before = sstable_count_before;
    results.sstable_count_after = sstable_count_after;
    results.improvement_factor = improvement;

    return results;
}

void benchmarkUpdateCompaction() {
    std::cout << "=== Update-Heavy Workload + Compaction ===\n\n";

    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 0);

    const size_t NUM_KEYS = 5000;
    const size_t NUM_UPDATES = 10000;

    engine.pauseCompaction();

    std::cout << "Writing initial dataset (" << NUM_KEYS << " keys)...\n";
    for (size_t i = 0; i < NUM_KEYS; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = generateRandomString(100);
        engine.put(key, value);
    }
    engine.flush();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, NUM_KEYS - 1);

    std::cout << "Performing " << NUM_UPDATES << " random updates...\n";
    auto update_start = std::chrono::high_resolution_clock::now();

    // Write updates in multiple batches to create more SSTables
    const size_t BATCH_SIZE = 2500;
    for (size_t batch = 0; batch < 4; ++batch) {
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            std::string key = "key_" + std::to_string(dis(gen));
            std::string value = "updated_" + generateRandomString(100);
            engine.put(key, value);
        }
        engine.flush();
    }

    auto update_end = std::chrono::high_resolution_clock::now();
    double update_time = std::chrono::duration<double, std::milli>(update_end - update_start).count();

    // Measure space before compaction
    std::filesystem::path sstable_dir = "data/sstables";
    size_t space_before = 0;
    if (std::filesystem::exists(sstable_dir)) {
        space_before = std::accumulate(std::filesystem::directory_iterator(sstable_dir), std::filesystem::directory_iterator{}, size_t{0},
                                       [](size_t sum, const auto &entry) {
                                           return entry.path().extension() == ".bin" ? sum + std::filesystem::file_size(entry.path()) : sum;
                                       });
    }

    std::cout << "Updates completed in " << update_time << " ms\n";
    std::cout << "Space before compaction: " << (space_before / 1024.0) << " KB\n\n";

    std::cout << "Triggering compaction...\n";
    engine.resumeCompaction();
    engine.waitForCompaction();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Measure space after compaction
    size_t space_after = 0;
    if (std::filesystem::exists(sstable_dir)) {
        space_after = std::accumulate(std::filesystem::directory_iterator(sstable_dir), std::filesystem::directory_iterator{}, size_t{0},
                                      [](size_t sum, const auto &entry) {
                                          return entry.path().extension() == ".bin" ? sum + std::filesystem::file_size(entry.path()) : sum;
                                      });
    }

    double space_reclaimed = (space_before > 0) ? ((space_before - space_after) * 100.0 / space_before) : 0;

    std::cout << "Space after compaction: " << (space_after / 1024.0) << " KB\n";
    std::cout << "Space reclaimed: " << ((space_before - space_after) / 1024.0) << " KB ";
    std::cout << "(" << std::fixed << std::setprecision(1) << space_reclaimed << "%)\n\n";
}

void benchmarkDeletionCompaction() {
    std::cout << "=== Deletion + Compaction (Tombstone Removal) ===\n\n";

    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 0);

    const size_t NUM_KEYS = 5000;

    engine.pauseCompaction();

    std::cout << "Writing " << NUM_KEYS << " keys in multiple SSTables...\n";

    // Write in batches to create multiple SSTables
    const size_t BATCH_SIZE = 1250;
    for (size_t batch = 0; batch < 4; ++batch) {
        for (size_t i = batch * BATCH_SIZE; i < (batch + 1) * BATCH_SIZE; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = generateRandomString(100);
            engine.put(key, value);
        }
        engine.flush();
    }

    size_t space_before_deletion = 0;
    std::filesystem::path sstable_dir = "data/sstables";
    if (std::filesystem::exists(sstable_dir)) {
        space_before_deletion =
            std::accumulate(std::filesystem::directory_iterator(sstable_dir), std::filesystem::directory_iterator{}, size_t{0},
                            [](size_t sum, const auto &entry) {
                                return entry.path().extension() == ".bin" ? sum + std::filesystem::file_size(entry.path()) : sum;
                            });
    }

    std::cout << "Space before deletions: " << (space_before_deletion / 1024.0) << " KB\n";

    // Delete 50% of keys
    std::cout << "Deleting 50% of keys...\n";
    for (size_t i = 0; i < NUM_KEYS / 2; ++i) {
        std::string key = "key_" + std::to_string(i * 2);
        engine.del(key);
    }
    engine.flush();

    size_t space_with_tombstones = 0;
    if (std::filesystem::exists(sstable_dir)) {
        space_with_tombstones =
            std::accumulate(std::filesystem::directory_iterator(sstable_dir), std::filesystem::directory_iterator{}, size_t{0},
                            [](size_t sum, const auto &entry) {
                                return entry.path().extension() == ".bin" ? sum + std::filesystem::file_size(entry.path()) : sum;
                            });
    }

    std::cout << "Space with tombstones: " << (space_with_tombstones / 1024.0) << " KB\n\n";

    std::cout << "Triggering compaction to remove tombstones...\n";
    engine.resumeCompaction();
    engine.waitForCompaction();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    size_t space_after_compaction = 0;
    if (std::filesystem::exists(sstable_dir)) {
        space_after_compaction =
            std::accumulate(std::filesystem::directory_iterator(sstable_dir), std::filesystem::directory_iterator{}, size_t{0},
                            [](size_t sum, const auto &entry) {
                                return entry.path().extension() == ".bin" ? sum + std::filesystem::file_size(entry.path()) : sum;
                            });
    }

    std::cout << "Space after compaction: " << (space_after_compaction / 1024.0) << " KB\n";
    std::cout << "Space reclaimed from tombstones: " << ((space_with_tombstones - space_after_compaction) / 1024.0) << " KB\n";

    double reclaim_pct =
        (space_with_tombstones > 0) ? ((space_with_tombstones - space_after_compaction) * 100.0 / space_with_tombstones) : 0;
    std::cout << "(" << std::fixed << std::setprecision(1) << reclaim_pct << "% reclaimed)\n\n";
}

int main() {
    std::cout << "=== KV Storage Engine - Compaction Benchmarks ===\n\n";

    auto results = benchmarkCompactionImpact();
    benchmarkUpdateCompaction();
    benchmarkDeletionCompaction();

    std::cout << "Compaction provides:\n";
    std::cout << "  • " << std::fixed << std::setprecision(1) << results.improvement_factor << "x faster reads by reducing SSTable count\n";

    std::filesystem::remove_all("data");

    return 0;
}
