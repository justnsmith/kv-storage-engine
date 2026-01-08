#include "engine.h"
#include <atomic>
#include <chrono>
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

// Benchmark concurrent writes
BenchmarkResult benchmarkConcurrentWrites(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t value_size) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};
    std::vector<std::vector<double>> thread_latencies(num_threads);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());
            std::vector<double> &latencies = thread_latencies[t];
            latencies.reserve(ops_per_thread);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "t" + std::to_string(t) + "_key_" + std::to_string(i);
                std::string value = generateRandomString(value_size, gen);

                auto op_start = std::chrono::high_resolution_clock::now();
                engine.put(key, value);
                auto op_end = std::chrono::high_resolution_clock::now();

                double latency_us = std::chrono::duration<double, std::micro>(op_end - op_start).count();
                latencies.push_back(latency_us);
                total_ops.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Aggregate latencies
    std::vector<double> all_latencies;
    for (const auto &tl : thread_latencies) {
        all_latencies.insert(all_latencies.end(), tl.begin(), tl.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    double avg_latency = 0;
    avg_latency = std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0);
    avg_latency /= all_latencies.size();

    double p99_latency = all_latencies[static_cast<size_t>(all_latencies.size() * 0.99)];

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = avg_latency;
    result.latency_p99_us = p99_latency;

    return result;
}

// Benchmark concurrent reads
BenchmarkResult benchmarkConcurrentReads(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t total_keys) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};
    std::atomic<size_t> hits{0};
    std::vector<std::vector<double>> thread_latencies(num_threads);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());
            std::uniform_int_distribution<size_t> key_dis(0, total_keys - 1);
            std::vector<double> &latencies = thread_latencies[t];
            latencies.reserve(ops_per_thread);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(key_dis(gen));

                auto op_start = std::chrono::high_resolution_clock::now();
                Entry result;
                bool found = engine.get(key, result);
                auto op_end = std::chrono::high_resolution_clock::now();

                if (found) {
                    hits.fetch_add(1, std::memory_order_relaxed);
                }

                double latency_us = std::chrono::duration<double, std::micro>(op_end - op_start).count();
                latencies.push_back(latency_us);
                total_ops.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Aggregate latencies
    std::vector<double> all_latencies;
    for (const auto &tl : thread_latencies) {
        all_latencies.insert(all_latencies.end(), tl.begin(), tl.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    double avg_latency = 0;
    avg_latency = std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0);
    avg_latency /= all_latencies.size();

    double p99_latency = all_latencies[static_cast<size_t>(all_latencies.size() * 0.99)];

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = avg_latency;
    result.latency_p99_us = p99_latency;

    return result;
}

// Benchmark mixed read/write workload
BenchmarkResult benchmarkMixedWorkload(StorageEngine &engine, size_t num_threads, size_t ops_per_thread, size_t value_size,
                                       size_t total_keys, int read_pct) {
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops{0};
    std::atomic<size_t> read_ops{0};
    std::atomic<size_t> write_ops{0};
    std::vector<std::vector<double>> thread_latencies(num_threads);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t * 12345 + std::random_device{}());
            std::uniform_int_distribution<size_t> key_dis(0, total_keys - 1);
            std::uniform_int_distribution<int> op_dis(0, 99);
            std::vector<double> &latencies = thread_latencies[t];
            latencies.reserve(ops_per_thread);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(key_dis(gen));

                auto op_start = std::chrono::high_resolution_clock::now();

                if (op_dis(gen) < read_pct) {
                    Entry result;
                    engine.get(key, result);
                    read_ops.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::string value = generateRandomString(value_size, gen);
                    engine.put(key, value);
                    write_ops.fetch_add(1, std::memory_order_relaxed);
                }

                auto op_end = std::chrono::high_resolution_clock::now();

                double latency_us = std::chrono::duration<double, std::micro>(op_end - op_start).count();
                latencies.push_back(latency_us);
                total_ops.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Aggregate latencies
    std::vector<double> all_latencies;
    for (const auto &tl : thread_latencies) {
        all_latencies.insert(all_latencies.end(), tl.begin(), tl.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    double avg_latency = 0;

    avg_latency = std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0);
    avg_latency /= all_latencies.size();

    double p99_latency = all_latencies[static_cast<size_t>(all_latencies.size() * 0.99)];

    BenchmarkResult result;
    result.total_ops = total_ops.load();
    result.duration_ms = duration_ms;
    result.throughput_ops_sec = (result.total_ops * 1000.0) / duration_ms;
    result.latency_avg_us = avg_latency;
    result.latency_p99_us = p99_latency;

    return result;
}

void printHeader(const std::string &title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void printResult(size_t threads, const BenchmarkResult &result) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << std::setw(2) << threads << " threads: ";
    std::cout << std::setw(10) << result.throughput_ops_sec << " ops/sec | ";
    std::cout << "avg: " << std::setw(8) << result.latency_avg_us << " µs | ";
    std::cout << "p99: " << std::setw(8) << result.latency_p99_us << " µs | ";
    std::cout << "total: " << result.total_ops << " ops in " << result.duration_ms << " ms\n";
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
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       Multi-threaded Storage Engine Benchmark            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    const std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
    const size_t ops_per_thread = 10000;
    const size_t value_size = 100;
    const size_t total_keys = 100000;

    printHeader("Benchmark 1: Concurrent Writes");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << value_size << "B values\n\n";

    std::vector<BenchmarkResult> write_results;
    for (size_t threads : thread_counts) {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 0);

        auto result = benchmarkConcurrentWrites(engine, threads, ops_per_thread, value_size);
        write_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, write_results);

    printHeader("Benchmark 2: Concurrent Reads");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << total_keys << " keys pre-loaded\n\n";

    // Pre-populate data once
    {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 1000);
        std::mt19937 gen(42);
        for (size_t i = 0; i < total_keys; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = generateRandomString(value_size, gen);
            engine.put(key, value);
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

    printHeader("Benchmark 3: Mixed Workload (70% reads, 30% writes)");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << total_keys << " key range\n\n";

    std::vector<BenchmarkResult> mixed_results;
    for (size_t threads : thread_counts) {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 1000);

        // Pre-populate with half the keys
        std::mt19937 gen(42);
        for (size_t i = 0; i < total_keys / 2; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = generateRandomString(value_size, gen);
            engine.put(key, value);
        }

        auto result = benchmarkMixedWorkload(engine, threads, ops_per_thread, value_size, total_keys, 70);
        mixed_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, mixed_results);

    printHeader("Benchmark 4: Write-heavy Workload (20% reads, 80% writes)");
    std::cout << "  Config: " << ops_per_thread << " ops/thread, " << total_keys << " key range\n\n";

    std::vector<BenchmarkResult> write_heavy_results;
    for (size_t threads : thread_counts) {
        std::filesystem::remove_all("data");
        StorageEngine engine("data/log.bin", 1000);

        auto result = benchmarkMixedWorkload(engine, threads, ops_per_thread, value_size, total_keys, 20);
        write_heavy_results.push_back(result);
        printResult(threads, result);
    }
    printScalingSummary(thread_counts, write_heavy_results);

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Summary: Peak Throughput at 16 threads\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << std::fixed << std::setprecision(0);
    std::cout << "  Writes only:     " << std::setw(10) << write_results.back().throughput_ops_sec << " ops/sec\n";
    std::cout << "  Reads only:      " << std::setw(10) << read_results.back().throughput_ops_sec << " ops/sec\n";
    std::cout << "  Mixed (70r/30w): " << std::setw(10) << mixed_results.back().throughput_ops_sec << " ops/sec\n";
    std::cout << "  Write-heavy:     " << std::setw(10) << write_heavy_results.back().throughput_ops_sec << " ops/sec\n";

    std::filesystem::remove_all("data");

    return 0;
}
