#include "lru_cache.h"
#include "test_framework.h"
#include <chrono>
#include <thread>
#include <vector>

class LRUCacheTest {
  public:
    LRUCacheTest() {
        setUp();
    }

    static void setUp() {
        // Tests will create their own caches as needed
    }
};

bool test_basic_put_and_get(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry1{"value1", 1, EntryType::PUT};
    cache.put("key1", entry1);

    auto result = cache.get("key1");
    ASSERT_TRUE(result.has_value(), "Should find key1 in cache");
    ASSERT_EQ(result->value, "value1", "Value should match");
    ASSERT_EQ(result->seq, 1, "Sequence number should match");
    ASSERT_TRUE(result->type == EntryType::PUT, "Entry type should match");

    return true;
}

bool test_get_nonexistent_key(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    auto result = cache.get("nonexistent");
    ASSERT_TRUE(!result.has_value(), "Should not find nonexistent key");

    return true;
}

bool test_update_existing_key(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry1{"value1", 1, EntryType::PUT};
    cache.put("key1", entry1);

    Entry entry2{"value2", 2, EntryType::PUT};
    cache.put("key1", entry2);

    auto result = cache.get("key1");
    ASSERT_TRUE(result.has_value(), "Should find updated key");
    ASSERT_EQ(result->value, "value2", "Value should be updated");
    ASSERT_EQ(result->seq, 2, "Sequence number should be updated");

    return true;
}

bool test_capacity_limit(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(3);

    Entry entry1{"value1", 1, EntryType::PUT};
    Entry entry2{"value2", 2, EntryType::PUT};
    Entry entry3{"value3", 3, EntryType::PUT};
    Entry entry4{"value4", 4, EntryType::PUT};

    cache.put("key1", entry1);
    cache.put("key2", entry2);
    cache.put("key3", entry3);

    ASSERT_EQ(cache.size(), 3, "Cache size should be at capacity");

    // This should evict key1 (least recently used)
    cache.put("key4", entry4);

    ASSERT_EQ(cache.size(), 3, "Cache size should remain at capacity");

    auto result1 = cache.get("key1");
    ASSERT_TRUE(!result1.has_value(), "key1 should be evicted");

    auto result4 = cache.get("key4");
    ASSERT_TRUE(result4.has_value(), "key4 should be in cache");

    return true;
}

bool test_lru_eviction_order(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(3);

    Entry entry1{"value1", 1, EntryType::PUT};
    Entry entry2{"value2", 2, EntryType::PUT};
    Entry entry3{"value3", 3, EntryType::PUT};
    Entry entry4{"value4", 4, EntryType::PUT};

    cache.put("key1", entry1);
    cache.put("key2", entry2);
    cache.put("key3", entry3);

    // Access key1 to make it recently used
    cache.get("key1");

    // Now key2 is the least recently used
    // Adding key4 should evict key2
    cache.put("key4", entry4);

    auto result1 = cache.get("key1");
    ASSERT_TRUE(result1.has_value(), "key1 should still be in cache (was accessed)");

    auto result2 = cache.get("key2");
    ASSERT_TRUE(!result2.has_value(), "key2 should be evicted (least recently used)");

    auto result3 = cache.get("key3");
    ASSERT_TRUE(result3.has_value(), "key3 should still be in cache");

    auto result4 = cache.get("key4");
    ASSERT_TRUE(result4.has_value(), "key4 should be in cache");

    return true;
}

bool test_put_updates_recency(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(3);

    Entry entry1{"value1", 1, EntryType::PUT};
    Entry entry2{"value2", 2, EntryType::PUT};
    Entry entry3{"value3", 3, EntryType::PUT};
    Entry entry4{"value4", 4, EntryType::PUT};

    cache.put("key1", entry1);
    cache.put("key2", entry2);
    cache.put("key3", entry3);

    // Update key1 (should move it to front)
    Entry entry1_updated{"value1_updated", 5, EntryType::PUT};
    cache.put("key1", entry1_updated);

    // Now key2 is least recently used
    cache.put("key4", entry4);

    auto result1 = cache.get("key1");
    ASSERT_TRUE(result1.has_value(), "key1 should still be in cache (was updated)");
    ASSERT_EQ(result1->value, "value1_updated", "Value should be updated");

    auto result2 = cache.get("key2");
    ASSERT_TRUE(!result2.has_value(), "key2 should be evicted");

    return true;
}

bool test_clear(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry1{"value1", 1, EntryType::PUT};
    Entry entry2{"value2", 2, EntryType::PUT};
    Entry entry3{"value3", 3, EntryType::PUT};

    cache.put("key1", entry1);
    cache.put("key2", entry2);
    cache.put("key3", entry3);

    ASSERT_EQ(cache.size(), 3, "Cache should contain 3 items");

    cache.clear();

    ASSERT_EQ(cache.size(), 0, "Cache should be empty after clear");

    auto result1 = cache.get("key1");
    ASSERT_TRUE(!result1.has_value(), "key1 should not be found after clear");

    auto result2 = cache.get("key2");
    ASSERT_TRUE(!result2.has_value(), "key2 should not be found after clear");

    return true;
}

bool test_invalidate(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry1{"value1", 1, EntryType::PUT};
    Entry entry2{"value2", 2, EntryType::PUT};
    Entry entry3{"value3", 3, EntryType::PUT};

    cache.put("key1", entry1);
    cache.put("key2", entry2);
    cache.put("key3", entry3);

    ASSERT_EQ(cache.size(), 3, "Cache should contain 3 items");

    cache.invalidate("key2");

    ASSERT_EQ(cache.size(), 2, "Cache should contain 2 items after invalidation");

    auto result1 = cache.get("key1");
    ASSERT_TRUE(result1.has_value(), "key1 should still be in cache");

    auto result2 = cache.get("key2");
    ASSERT_TRUE(!result2.has_value(), "key2 should be invalidated");

    auto result3 = cache.get("key3");
    ASSERT_TRUE(result3.has_value(), "key3 should still be in cache");

    return true;
}

bool test_invalidate_nonexistent_key(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry1{"value1", 1, EntryType::PUT};
    cache.put("key1", entry1);

    ASSERT_EQ(cache.size(), 1, "Cache should contain 1 item");

    // Should not throw or crash
    cache.invalidate("nonexistent");

    ASSERT_EQ(cache.size(), 1, "Cache size should remain unchanged");

    return true;
}

bool test_size(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    ASSERT_EQ(cache.size(), 0, "Empty cache should have size 0");

    Entry entry1{"value1", 1, EntryType::PUT};
    cache.put("key1", entry1);
    ASSERT_EQ(cache.size(), 1, "Cache should have size 1");

    Entry entry2{"value2", 2, EntryType::PUT};
    cache.put("key2", entry2);
    ASSERT_EQ(cache.size(), 2, "Cache should have size 2");

    cache.invalidate("key1");
    ASSERT_EQ(cache.size(), 1, "Cache should have size 1 after invalidation");

    cache.clear();
    ASSERT_EQ(cache.size(), 0, "Cache should have size 0 after clear");

    return true;
}

bool test_empty_string_key(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry{"value_for_empty_key", 1, EntryType::PUT};
    cache.put("", entry);

    auto result = cache.get("");
    ASSERT_TRUE(result.has_value(), "Should find empty string key");
    ASSERT_EQ(result->value, "value_for_empty_key", "Value should match");

    return true;
}

bool test_empty_value(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry{"", 1, EntryType::PUT};
    cache.put("key1", entry);

    auto result = cache.get("key1");
    ASSERT_TRUE(result.has_value(), "Should find key with empty value");
    ASSERT_EQ(result->value, "", "Value should be empty");

    return true;
}

bool test_delete_entry_type(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry{"", 1, EntryType::DELETE};
    cache.put("key1", entry);

    auto result = cache.get("key1");
    ASSERT_TRUE(result.has_value(), "Should find DELETE entry");
    ASSERT_TRUE(result->type == EntryType::DELETE, "Entry type should be DELETE");
    ASSERT_EQ(result->value, "", "DELETE entry should have empty value");

    return true;
}

bool test_special_characters_in_key(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    Entry entry1{"value1", 1, EntryType::PUT};
    Entry entry2{"value2", 2, EntryType::PUT};
    Entry entry3{"value3", 3, EntryType::PUT};

    cache.put("key!@#$%", entry1);
    cache.put("key\n\t", entry2);
    cache.put("key with spaces", entry3);

    auto result1 = cache.get("key!@#$%");
    ASSERT_TRUE(result1.has_value(), "Should handle special characters");

    auto result2 = cache.get("key\n\t");
    ASSERT_TRUE(result2.has_value(), "Should handle whitespace characters");

    auto result3 = cache.get("key with spaces");
    ASSERT_TRUE(result3.has_value(), "Should handle spaces");

    return true;
}

bool test_long_keys_and_values(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10);

    std::string long_key(1000, 'k');
    std::string long_value(10000, 'v');
    Entry entry{long_value, 1, EntryType::PUT};

    cache.put(long_key, entry);

    auto result = cache.get(long_key);
    ASSERT_TRUE(result.has_value(), "Should handle long keys");
    ASSERT_EQ(result->value, long_value, "Long value should match");

    return true;
}

bool test_capacity_one(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(1);

    Entry entry1{"value1", 1, EntryType::PUT};
    Entry entry2{"value2", 2, EntryType::PUT};

    cache.put("key1", entry1);
    ASSERT_EQ(cache.size(), 1, "Cache should have size 1");

    cache.put("key2", entry2);
    ASSERT_EQ(cache.size(), 1, "Cache should still have size 1");

    auto result1 = cache.get("key1");
    ASSERT_TRUE(!result1.has_value(), "key1 should be evicted");

    auto result2 = cache.get("key2");
    ASSERT_TRUE(result2.has_value(), "key2 should be in cache");

    return true;
}

bool test_large_capacity(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(10000);

    // Add many entries
    for (int i = 0; i < 5000; i++) {
        Entry entry{"value" + std::to_string(i), static_cast<uint64_t>(i), EntryType::PUT};
        cache.put("key" + std::to_string(i), entry);
    }

    ASSERT_EQ(cache.size(), 5000, "Cache should contain all 5000 entries");

    // Verify random access
    auto result1 = cache.get("key100");
    ASSERT_TRUE(result1.has_value(), "Should find key100");
    ASSERT_EQ(result1->value, "value100", "Value should match");

    auto result2 = cache.get("key2500");
    ASSERT_TRUE(result2.has_value(), "Should find key2500");
    ASSERT_EQ(result2->value, "value2500", "Value should match");

    return true;
}

bool test_sequential_access_pattern(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(5);

    // Fill cache
    for (int i = 0; i < 5; i++) {
        Entry entry{"value" + std::to_string(i), static_cast<uint64_t>(i), EntryType::PUT};
        cache.put("key" + std::to_string(i), entry);
    }

    // Access in reverse order to change LRU order
    for (int i = 4; i >= 0; i--) {
        cache.get("key" + std::to_string(i));
    }

    // Add new entry - should evict key4 (now least recently used)
    Entry entry{"value5", 5, EntryType::PUT};
    cache.put("key5", entry);

    auto result4 = cache.get("key4");
    ASSERT_TRUE(!result4.has_value(), "key4 should be evicted");

    auto result0 = cache.get("key0");
    ASSERT_TRUE(result0.has_value(), "key0 should still be in cache");

    return true;
}

bool test_thread_safety_basic(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(1000);

    const int num_threads = 4;
    const int ops_per_thread = 100;
    std::vector<std::thread> threads;

    // Multiple threads inserting
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&cache, t]() {
            for (int i = 0; i < ops_per_thread; i++) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                Entry entry{"value_" + std::to_string(i), static_cast<uint64_t>(i), EntryType::PUT};
                cache.put(key, entry);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    // Cache should not crash and should have reasonable size
    ASSERT_TRUE(cache.size() <= 1000, "Cache size should not exceed capacity");
    ASSERT_TRUE(cache.size() > 0, "Cache should contain some entries");

    return true;
}

bool test_thread_safety_mixed_operations(LRUCacheTest &fixture) {
    fixture.setUp();
    LRUCache cache(500);

    const int num_threads = 4;
    const int ops_per_thread = 50;
    std::vector<std::thread> threads;

    // Multiple threads doing mixed operations
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&cache]() {
            for (int i = 0; i < ops_per_thread; i++) {
                std::string key = "shared_key_" + std::to_string(i % 10);

                if (i % 3 == 0) {
                    Entry entry{"value_" + std::to_string(i), static_cast<uint64_t>(i), EntryType::PUT};
                    cache.put(key, entry);
                } else if (i % 3 == 1) {
                    cache.get(key);
                } else {
                    cache.invalidate(key);
                }
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    // Should not crash - basic safety check
    ASSERT_TRUE(true, "Thread safety test completed without crash");

    return true;
}

void run_lru_cache_tests(TestFramework &framework) {
    LRUCacheTest fixture;

    std::cout << "Running LRU Cache Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    framework.run("test_basic_put_and_get", [&]() { return test_basic_put_and_get(fixture); });
    framework.run("test_get_nonexistent_key", [&]() { return test_get_nonexistent_key(fixture); });
    framework.run("test_update_existing_key", [&]() { return test_update_existing_key(fixture); });
    framework.run("test_capacity_limit", [&]() { return test_capacity_limit(fixture); });
    framework.run("test_lru_eviction_order", [&]() { return test_lru_eviction_order(fixture); });
    framework.run("test_put_updates_recency", [&]() { return test_put_updates_recency(fixture); });
    framework.run("test_clear", [&]() { return test_clear(fixture); });
    framework.run("test_invalidate", [&]() { return test_invalidate(fixture); });
    framework.run("test_invalidate_nonexistent_key", [&]() { return test_invalidate_nonexistent_key(fixture); });
    framework.run("test_size", [&]() { return test_size(fixture); });
    framework.run("test_empty_string_key", [&]() { return test_empty_string_key(fixture); });
    framework.run("test_empty_value", [&]() { return test_empty_value(fixture); });
    framework.run("test_delete_entry_type", [&]() { return test_delete_entry_type(fixture); });
    framework.run("test_special_characters_in_key", [&]() { return test_special_characters_in_key(fixture); });
    framework.run("test_long_keys_and_values", [&]() { return test_long_keys_and_values(fixture); });
    framework.run("test_capacity_one", [&]() { return test_capacity_one(fixture); });
    framework.run("test_large_capacity", [&]() { return test_large_capacity(fixture); });
    framework.run("test_sequential_access_pattern", [&]() { return test_sequential_access_pattern(fixture); });
    framework.run("test_thread_safety_basic", [&]() { return test_thread_safety_basic(fixture); });
    framework.run("test_thread_safety_mixed_operations", [&]() { return test_thread_safety_mixed_operations(fixture); });
}
