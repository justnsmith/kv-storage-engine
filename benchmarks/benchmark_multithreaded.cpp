#include "storage_engine.h"
#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

// Thread-local random string generator
std::string generateRandomString(size_t length, std::mt19937 &gen) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    return result;
}

struct BenchmarkResult {
    double throughput_ops_sec;
    double latency_avg_us;
    double latency_p99_us;
    size_t total_ops;
    double duration_ms;
};

// Benchmark concurrent writes using ASYNC API for true throughput
BenchmarkResult benchmarkConcurrentWrites(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t value_size) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());

            constexpr size_t BATCH_SIZE = 1000;
            std::vector<std::future<bool>> futures;
            futures.reserve(BATCH_SIZE);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "t" + std::to_string(t) + "_key_" + std::to_string(i);
                std::string value = generateRandomString(value_size, gen);

                futures.push_back(engine.putAsync(key, value));

                // Wait for batch completion periodically
                if (futures.size() >= BATCH_SIZE) {
                    for (auto &f : futures) {
                        f.get();
                    }
                    futures.clear();
                }
            }

            // Wait for remaining futures
            for (auto &f : futures) {
                f.get();
            }

            total_ops.fetch_add(ops_per_thread, std::memory_order_relaxed);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = 0;
    result.latency_p99_us = 0;

    return result;
}

// Benchmark concurrent writes using ASYNC API for true throughput (sequential keys)
BenchmarkResult benchmarkConcurrentWritesSequential(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t value_size) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());

            constexpr size_t BATCH_SIZE = 1000;
            std::vector<std::future<bool>> futures;
            futures.reserve(BATCH_SIZE);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "t" + std::to_string(t) + "_key_" + std::to_string(i);
                std::string value = generateRandomString(value_size, gen);

                futures.push_back(engine.putAsync(key, value));

                if (futures.size() >= BATCH_SIZE) {
                    for (auto &f : futures) {
                        f.get();
                    }
                    futures.clear();
                }
            }

            for (auto &f : futures) {
                f.get();
            }

            total_ops.fetch_add(ops_per_thread, std::memory_order_relaxed);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = 0;
    result.latency_p99_us = 0;

    return result;
}

// Benchmark concurrent writes using ASYNC API with random keys
BenchmarkResult benchmarkConcurrentWritesRandom(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t value_size,
                                                size_t key_range) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());
            std::uniform_int_distribution<size_t> key_dis(0, key_range - 1);

            constexpr size_t BATCH_SIZE = 1000;
            std::vector<std::future<bool>> futures;
            futures.reserve(BATCH_SIZE);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(key_dis(gen));
                std::string value = generateRandomString(value_size, gen);

                futures.push_back(engine.putAsync(key, value));

                if (futures.size() >= BATCH_SIZE) {
                    for (auto &f : futures) {
                        f.get();
                    }
                    futures.clear();
                }
            }

            for (auto &f : futures) {
                f.get();
            }

            total_ops.fetch_add(ops_per_thread, std::memory_order_relaxed);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = 0;
    result.latency_p99_us = 0;

    return result;
}

// Benchmark concurrent reads (no async needed for reads)
BenchmarkResult benchmarkConcurrentReads(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t total_keys) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};
    std::atomic<size_t> hits{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());
            std::uniform_int_distribution<size_t> key_dis(0, total_keys - 1);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(key_dis(gen));
                Entry result;
                bool found = engine.get(key, result);

                if (found) {
                    hits.fetch_add(1, std::memory_order_relaxed);
                }
                total_ops.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = 0;
    result.latency_p99_us = 0;

    return result;
}

// Benchmark mixed read/write workload using async writes
BenchmarkResult benchmarkMixedWorkload(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t value_size,
                                       size_t total_keys, int read_pct) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};
    std::atomic<size_t> read_ops{0};
    std::atomic<size_t> write_ops{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());
            std::uniform_int_distribution<size_t> key_dis(0, total_keys - 1);
            std::uniform_int_distribution<int> op_dis(0, 99);

            constexpr size_t BATCH_SIZE = 1000;
            std::vector<std::future<bool>> futures;
            futures.reserve(BATCH_SIZE);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(key_dis(gen));

                if (op_dis(gen) < read_pct) {
                    Entry result;
                    engine.get(key, result);
                    read_ops.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::string value = generateRandomString(value_size, gen);
                    futures.push_back(engine.putAsync(key, value));
                    write_ops.fetch_add(1, std::memory_order_relaxed);

                    if (futures.size() >= BATCH_SIZE) {
                        for (auto &f : futures) {
                            f.get();
                        }
                        futures.clear();
                    }
                }

                total_ops.fetch_add(1, std::memory_order_relaxed);
            }

            // Wait for remaining futures
            for (auto &f : futures) {
                f.get();
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = 0;
    result.latency_p99_us = 0;

    return result;
}

void printHeader(const std::string &title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n";
}

void printResult(size_t threads, const BenchmarkResult &result) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << std::setw(2) << threads << " threads: ";
    std::cout << std::setw(12) << result.throughput_ops_sec << " ops/sec";
    std::cout << " | " << result.total_ops << " ops in " << static_cast<int>(result.duration_ms) << " ms\n";
}

void printScalingSummary(const std::vector<size_t> &thread_counts, const std::vector<BenchmarkResult> &results) {
    std::cout << "\n  Scaling efficiency (vs 1 thread):\n";
    double baseline = results[0].throughput_ops_sec;
    for (size_t i = 0; i < thread_counts.size(); ++i) {
        double speedup = results[i].throughput_ops_sec / baseline;
        double efficiency = (speedup / thread_counts[i]) * 100;
        std::cout << "    " << std::setw(2) << thread_counts[i] << " threads: ";
        std::cout << std::setw(5) << std::fixed << std::setprecision(2) << speedup << "x speedup, ";
        std::cout << std::setw(5) << efficiency << "% efficiency\n";
    }
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          Multi-threaded Storage Engine Benchmark                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════╝\n";

    const std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
    const size_t ops_per_thread = 100000;
    const size_t value_size = 100;
    const size_t total_keys = 100000;

    printHeader("Benchmark 1: Concurrent Writes - Sequential Keys");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << value_size << "B values\n";
    std::cout << "  Each thread writes to unique key space (no contention)\n\n";

    std::vector<BenchmarkResult> write_seq_results;
    for (size_t threads : thread_counts) {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 0);

        auto result = benchmarkConcurrentWritesSequential(engine, threads, ops_per_thread, value_size);
        write_seq_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, write_seq_results);

    printHeader("Benchmark 2: Concurrent Writes - Random Keys");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << value_size << "B values\n";
    std::cout << "  Key range: " << total_keys << " (overlapping writes)\n\n";

    std::vector<BenchmarkResult> write_rand_results;
    for (size_t threads : thread_counts) {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 0);

        auto result = benchmarkConcurrentWritesRandom(engine, threads, ops_per_thread, value_size, total_keys);
        write_rand_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, write_rand_results);

    printHeader("Benchmark 3: Concurrent Reads");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << total_keys << " keys pre-loaded\n\n";

    // Pre-populate data once
    {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 1000);
        std::mt19937 gen(42);

        constexpr size_t BATCH_SIZE = 1000;
        std::vector<std::future<bool>> futures;
        futures.reserve(BATCH_SIZE);

        for (size_t i = 0; i < total_keys; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = generateRandomString(value_size, gen);
            futures.push_back(engine.putAsync(key, value));

            if (futures.size() >= BATCH_SIZE) {
                for (auto &f : futures) {
                    f.get();
                }
                futures.clear();
            }
        }

        for (auto &f : futures) {
            f.get();
        }
    }

    std::vector<BenchmarkResult> read_results;
    for (size_t threads : thread_counts) {
        StorageEngine engine("data/log.bin", 1000);
        engine.recover();

        auto result = benchmarkConcurrentReads(engine, threads, ops_per_thread, total_keys);
        read_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, read_results);

    printHeader("Benchmark 4: Mixed Workload (70% reads, 30% writes)");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << total_keys << " key range\n\n";

    std::vector<BenchmarkResult> mixed_70_results;
    for (size_t threads : thread_counts) {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 1000);

        // Pre-populate with half the keys
        std::mt19937 gen(42);
        constexpr size_t BATCH_SIZE = 1000;
        std::vector<std::future<bool>> futures;
        futures.reserve(BATCH_SIZE);

        for (size_t i = 0; i < total_keys / 2; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = generateRandomString(value_size, gen);
            futures.push_back(engine.putAsync(key, value));

            if (futures.size() >= BATCH_SIZE) {
                for (auto &f : futures) {
                    f.get();
                }
                futures.clear();
            }
        }

        for (auto &f : futures) {
            f.get();
        }

        auto result = benchmarkMixedWorkload(engine, threads, ops_per_thread, value_size, total_keys, 70);
        mixed_70_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, mixed_70_results);

    printHeader("Benchmark 5: Write-heavy Workload (20% reads, 80% writes)");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << total_keys << " key range\n\n";

    std::vector<BenchmarkResult> mixed_20_results;
    for (size_t threads : thread_counts) {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 1000);

        auto result = benchmarkMixedWorkload(engine, threads, ops_per_thread, value_size, total_keys, 20);
        mixed_20_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, mixed_20_results);

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Summary: Peak Throughput at 16 threads\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << std::fixed << std::setprecision(0);
    std::cout << "  Sequential writes:    " << std::setw(12) << write_seq_results.back().throughput_ops_sec << " ops/sec\n";
    std::cout << "  Random writes:        " << std::setw(12) << write_rand_results.back().throughput_ops_sec << " ops/sec\n";
    std::cout << "  Reads only:           " << std::setw(12) << read_results.back().throughput_ops_sec << " ops/sec\n";
    std::cout << "  Mixed (70r/30w):      " << std::setw(12) << mixed_70_results.back().throughput_ops_sec << " ops/sec\n";
    std::cout << "  Write-heavy (20r/80w):" << std::setw(12) << mixed_20_results.back().throughput_ops_sec << " ops/sec\n";

    std::filesystem::remove_all("data");

    return 0;
}
