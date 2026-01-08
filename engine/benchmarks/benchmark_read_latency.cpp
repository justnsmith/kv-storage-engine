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

struct LatencyStats {
    double min;
    double max;
    double mean;
    double p50;
    double p95;
    double p99;
    double p999;
};

LatencyStats calculateStats(std::vector<double> &latencies) {
    std::sort(latencies.begin(), latencies.end());

    LatencyStats stats;
    stats.min = latencies.front();
    stats.max = latencies.back();

    double sum = 0;
    sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    stats.mean = sum / latencies.size();

    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(latencies.size() * p);
        return latencies[std::min(idx, latencies.size() - 1)];
    };

    stats.p50 = percentile(0.50);
    stats.p95 = percentile(0.95);
    stats.p99 = percentile(0.99);
    stats.p999 = percentile(0.999);

    return stats;
}

void printStats(const std::string &name, const LatencyStats &stats) {
    std::cout << name << ":\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Min:    " << stats.min << " μs\n";
    std::cout << "  Mean:   " << stats.mean << " μs\n";
    std::cout << "  P50:    " << stats.p50 << " μs\n";
    std::cout << "  P95:    " << stats.p95 << " μs\n";
    std::cout << "  P99:    " << stats.p99 << " μs\n";
    std::cout << "  P99.9:  " << stats.p999 << " μs\n";
    std::cout << "  Max:    " << stats.max << " μs\n\n";
}

void benchmarkMemTableReads(size_t num_keys, size_t num_reads) {
    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 0);

    // Insert data into memtable (without flushing)
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = generateRandomString(100);
        engine.put(key, value);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_keys - 1);

    std::vector<double> latencies;
    latencies.reserve(num_reads);

    for (size_t i = 0; i < num_reads; ++i) {
        std::string key = "key_" + std::to_string(dis(gen));
        Entry result;

        auto start = std::chrono::high_resolution_clock::now();
        engine.get(key, result);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    auto stats = calculateStats(latencies);
    printStats("MemTable Reads (Hot Data)", stats);
}

void benchmarkSSTableReads(size_t num_keys, size_t num_reads) {
    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 0);

    // Insert and flush to create SSTables
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = generateRandomString(100);
        engine.put(key, value);
    }
    engine.flush();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_keys - 1);

    std::vector<double> latencies;
    latencies.reserve(num_reads);

    for (size_t i = 0; i < num_reads; ++i) {
        std::string key = "key_" + std::to_string(dis(gen));
        Entry result;

        auto start = std::chrono::high_resolution_clock::now();
        engine.get(key, result);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    auto stats = calculateStats(latencies);
    printStats("SSTable Reads (Cold Data, No Cache)", stats);
}

void benchmarkCachedReads(size_t num_keys, size_t num_reads) {
    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 1000);

    // Insert and flush
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = generateRandomString(100);
        engine.put(key, value);
    }
    engine.flush();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, std::min(num_keys, size_t(500)) - 1);

    // Warm up cache
    for (size_t i = 0; i < 100; ++i) {
        std::string key = "key_" + std::to_string(dis(gen));
        Entry result;
        engine.get(key, result);
    }

    std::vector<double> latencies;
    latencies.reserve(num_reads);

    for (size_t i = 0; i < num_reads; ++i) {
        std::string key = "key_" + std::to_string(dis(gen));
        Entry result;

        auto start = std::chrono::high_resolution_clock::now();
        engine.get(key, result);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    auto stats = calculateStats(latencies);
    printStats("Cached Reads (Hot Working Set)", stats);
}

void benchmarkNonExistentKeys(size_t num_keys, size_t num_reads) {
    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 0);

    // Insert some data
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = generateRandomString(100);
        engine.put(key, value);
    }
    engine.flush();

    std::vector<double> latencies;
    latencies.reserve(num_reads);

    for (size_t i = 0; i < num_reads; ++i) {
        std::string key = "nonexistent_" + std::to_string(i);
        Entry result;

        auto start = std::chrono::high_resolution_clock::now();
        engine.get(key, result);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    auto stats = calculateStats(latencies);
    printStats("Non-Existent Key Reads (Bloom Filter Test)", stats);
}

int main() {
    std::cout << "=== KV Storage Engine - Read Latency Benchmarks ===\n\n";

    const size_t NUM_KEYS = 5000;
    const size_t NUM_READS = 1000;

    benchmarkMemTableReads(NUM_KEYS, NUM_READS);
    benchmarkSSTableReads(NUM_KEYS, NUM_READS);
    benchmarkCachedReads(NUM_KEYS, NUM_READS);
    benchmarkNonExistentKeys(NUM_KEYS, NUM_READS);

    std::filesystem::remove_all("data");

    return 0;
}
