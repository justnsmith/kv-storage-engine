#include "engine.h"
#include <chrono>
#include <future>
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

void benchmarkSequentialWrites(size_t num_ops, size_t value_size) {
    std::filesystem::remove_all("data");
    StorageEngine engine("data", 0);

    // Pre-generate all keys and values
    std::vector<std::string> keys, values;
    keys.reserve(num_ops);
    values.reserve(num_ops);
    for (size_t i = 0; i < num_ops; ++i) {
        keys.push_back("key_" + std::to_string(i));
        values.push_back(generateRandomString(value_size));
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Use async API with batched futures
    constexpr size_t BATCH_SIZE = 1000;
    std::vector<std::future<bool>> futures;
    futures.reserve(BATCH_SIZE);

    for (size_t i = 0; i < num_ops; ++i) {
        futures.push_back(engine.putAsync(keys[i], values[i]));

        // Wait for batch completion periodically to avoid unbounded memory
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

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double throughput = (num_ops * 1000.0) / duration.count();
    double mb_per_sec = (num_ops * value_size * 1000.0) / (duration.count() * 1024 * 1024);

    std::cout << "Sequential Writes (" << value_size << "B values):\n";
    std::cout << "  Operations: " << num_ops << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
    std::cout << "  Bandwidth: " << mb_per_sec << " MB/sec\n\n";
}

void benchmarkRandomWrites(size_t num_ops, size_t value_size) {
    std::filesystem::remove_all("data");
    StorageEngine engine("data", 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_ops * 10);

    // Pre-generate all keys and values
    std::vector<std::string> keys, values;
    keys.reserve(num_ops);
    values.reserve(num_ops);
    for (size_t i = 0; i < num_ops; ++i) {
        keys.push_back("key_" + std::to_string(dis(gen)));
        values.push_back(generateRandomString(value_size));
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Use async API with batched futures
    constexpr size_t BATCH_SIZE = 1000;
    std::vector<std::future<bool>> futures;
    futures.reserve(BATCH_SIZE);

    for (size_t i = 0; i < num_ops; ++i) {
        futures.push_back(engine.putAsync(keys[i], values[i]));

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

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double throughput = (num_ops * 1000.0) / duration.count();
    double mb_per_sec = (num_ops * value_size * 1000.0) / (duration.count() * 1024 * 1024);

    std::cout << "Random Writes (" << value_size << "B values):\n";
    std::cout << "  Operations: " << num_ops << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
    std::cout << "  Bandwidth: " << mb_per_sec << " MB/sec\n\n";
}

void benchmarkMixedWorkload(size_t num_ops, size_t value_size) {
    std::filesystem::remove_all("data");
    StorageEngine engine("data", 1000);

    // Pre-populate with some data
    for (size_t i = 0; i < num_ops / 2; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = generateRandomString(value_size);
        engine.put(key, value);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dis(0, num_ops);
    std::uniform_int_distribution<> op_dis(0, 99);

    size_t writes = 0;
    size_t reads = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_ops; ++i) {
        std::string key = "key_" + std::to_string(key_dis(gen));

        // 70% reads, 30% writes
        if (op_dis(gen) < 70) {
            Entry result;
            engine.get(key, result);
            reads++;
        } else {
            std::string value = generateRandomString(value_size);
            engine.put(key, value);
            writes++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double throughput = (num_ops * 1000.0) / duration.count();

    std::cout << "Mixed Workload (70% reads, 30% writes):\n";
    std::cout << "  Total operations: " << num_ops << "\n";
    std::cout << "  Reads: " << reads << " | Writes: " << writes << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n\n";
}

int main() {
    std::cout << "=== KV Storage Engine - Write Throughput Benchmarks ===\n\n";

    benchmarkSequentialWrites(1000000, 100);  // 100B values
    benchmarkSequentialWrites(1000000, 1024); // 1KB values
    benchmarkSequentialWrites(1000000, 4096); // 4KB values

    benchmarkRandomWrites(1000000, 100);
    benchmarkRandomWrites(1000000, 1024);

    benchmarkMixedWorkload(1000000, 1024);

    std::filesystem::remove_all("data");

    return 0;
}
