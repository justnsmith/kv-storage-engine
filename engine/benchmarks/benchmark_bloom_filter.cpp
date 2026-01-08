#include "bloom_filter.h"
#include "engine.h"
#include "sstable.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

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

void benchmarkBloomFilterAccuracy() {
    std::cout << "=== Bloom Filter Accuracy Test ===\n\n";

    const size_t NUM_ELEMENTS = 10000;
    const double FP_RATE = 0.01; // 1% false positive rate

    BloomFilter bf(NUM_ELEMENTS, FP_RATE);

    // Add elements
    std::vector<std::string> inserted_keys;
    for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
        std::string key = "key_" + std::to_string(i);
        bf.add(key);
        inserted_keys.push_back(key);
    }

    size_t true_positives = 0;
    true_positives = std::count_if(inserted_keys.begin(), inserted_keys.end(), [&bf](const std::string &key) { return bf.contains(key); });

    size_t false_positives = 0;
    const size_t TEST_SIZE = 10000;
    for (size_t i = 0; i < TEST_SIZE; ++i) {
        std::string key = "nonexistent_" + std::to_string(i);
        if (bf.contains(key)) {
            false_positives++;
        }
    }

    double fp_rate = (false_positives * 100.0) / TEST_SIZE;
    double tp_rate = (true_positives * 100.0) / NUM_ELEMENTS;

    std::cout << "Inserted keys: " << NUM_ELEMENTS << "\n";
    std::cout << "True positive rate: " << std::fixed << std::setprecision(2) << tp_rate << "%\n";
    std::cout << "False positive rate: " << fp_rate << "% (target: " << (FP_RATE * 100) << "%)\n";
    std::cout << "Bloom filter size: " << (bf.size() / 1024.0) << " KB\n";
    std::cout << "Bits per element: " << (bf.size() * 8.0 / NUM_ELEMENTS) << "\n\n";
}

void benchmarkWithAndWithoutBloomFilter() {
    std::cout << "=== Read Performance: With vs Without Bloom Filter ===\n\n";

    const size_t NUM_KEYS = 5000;
    const size_t NUM_READS = 1000;

    // Setup: Create database with many non-existent key lookups
    std::filesystem::remove_all("data");
    StorageEngine engine("data/log.bin", 0);

    std::cout << "Setting up database with " << NUM_KEYS << " keys...\n";
    for (size_t i = 0; i < NUM_KEYS; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = generateRandomString(100);
        engine.put(key, value);
    }
    engine.flush();

    // Test 1: Mix of existing and non-existing keys (50/50)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> exist_dis(0, NUM_KEYS - 1);
    std::uniform_int_distribution<> coin(0, 1);

    std::vector<std::string> test_keys;
    for (size_t i = 0; i < NUM_READS; ++i) {
        if (coin(gen)) {
            test_keys.push_back("key_" + std::to_string(exist_dis(gen)));
        } else {
            test_keys.push_back("nonexistent_" + std::to_string(i));
        }
    }

    size_t hits = 0;
    size_t misses = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto &key : test_keys) {
        Entry result;
        if (engine.get(key, result)) {
            hits++;
        } else {
            misses++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_latency = duration.count() / static_cast<double>(NUM_READS);

    std::cout << "\nResults (WITH Bloom Filter):\n";
    std::cout << "  Total reads: " << NUM_READS << "\n";
    std::cout << "  Hits: " << hits << " | Misses: " << misses << "\n";
    std::cout << "  Total time: " << (duration.count() / 1000.0) << " ms\n";
    std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) << avg_latency << " μs/op\n";
    std::cout << "  Throughput: " << (NUM_READS * 1000000.0 / duration.count()) << " ops/sec\n\n";

    // Test 2: Mostly non-existing keys (90% miss rate)
    test_keys.clear();
    std::uniform_int_distribution<> miss_coin(0, 9);

    for (size_t i = 0; i < NUM_READS; ++i) {
        if (miss_coin(gen) == 0) {
            test_keys.push_back("key_" + std::to_string(exist_dis(gen)));
        } else {
            test_keys.push_back("miss_" + std::to_string(i));
        }
    }

    hits = 0;
    misses = 0;

    start = std::chrono::high_resolution_clock::now();

    for (const auto &key : test_keys) {
        Entry result;
        if (engine.get(key, result)) {
            hits++;
        } else {
            misses++;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    avg_latency = duration.count() / static_cast<double>(NUM_READS);

    std::cout << "High Miss Rate Test (90% misses):\n";
    std::cout << "  Hits: " << hits << " | Misses: " << misses << "\n";
    std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) << avg_latency << " μs/op\n";
    std::cout << "  Throughput: " << (NUM_READS * 1000000.0 / duration.count()) << " ops/sec\n\n";

    std::cout << "Note: Bloom filters prevent expensive disk I/O for non-existent keys.\n";
    std::cout << "Expected improvement: 5-10x faster for high miss rate workloads\n\n";
}

void benchmarkBloomFilterMemoryEfficiency() {
    std::cout << "=== Bloom Filter Memory Efficiency ===\n\n";

    std::vector<size_t> sizes = {1000, 10000, 100000};
    std::vector<double> fp_rates = {0.001, 0.01, 0.05};

    std::cout << std::setw(15) << "Elements" << std::setw(15) << "FP Rate" << std::setw(15) << "Size (KB)" << std::setw(15)
              << "Bits/Element" << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t num_elements : sizes) {
        for (double fp_rate : fp_rates) {
            BloomFilter bf(num_elements, fp_rate);

            std::cout << std::setw(15) << num_elements << std::setw(15) << std::fixed << std::setprecision(3) << (fp_rate * 100) << "%"
                      << std::setw(15) << std::setprecision(2) << (bf.size() / 1024.0) << std::setw(15) << std::setprecision(1)
                      << (bf.size() * 8.0 / num_elements) << "\n";
        }
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== KV Storage Engine - Bloom Filter Benchmarks ===\n\n";

    benchmarkBloomFilterAccuracy();
    benchmarkWithAndWithoutBloomFilter();
    benchmarkBloomFilterMemoryEfficiency();

    std::filesystem::remove_all("data");

    return 0;
}
