#include "storage_engine.h"
#include "test_framework.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

class StorageEngineTest {
  public:
    StorageEngineTest() : engine_("data/log.bin") {
    }

    void setUp() {
        engine_.clearData();
    }

    StorageEngine &getEngine() {
        return engine_;
    }

  private:
    StorageEngine engine_;
};

// Basic operations tests
bool test_simple_put_and_get(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    bool status = engine.put("user42", "123");
    ASSERT_TRUE(status, "PUT should succeed");

    Entry result;
    status = engine.get("user42", result);
    ASSERT_TRUE(status, "GET after PUT should succeed");
    ASSERT_EQ(result.value, "123", "Value should match what was put");

    return true;
}

bool test_update_existing_key(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("user42", "123");
    engine.put("user42", "new123");

    Entry result;
    bool status = engine.get("user42", result);
    ASSERT_TRUE(status, "GET after update should succeed");
    ASSERT_EQ(result.value, "new123", "Value should be updated");

    return true;
}

bool test_get_nonexistent_key(StorageEngineTest &fixture) {
    fixture.setUp();
    const auto &engine = fixture.getEngine();

    Entry result;
    bool status = engine.get("nonexistent", result);
    ASSERT_TRUE(!status, "GET on nonexistent key should return false");

    return true;
}

bool test_simple_delete(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("user42", "123");
    bool status = engine.del("user42");
    ASSERT_TRUE(status, "DELETE existing key should return true");

    Entry result;
    status = engine.get("user42", result);
    ASSERT_TRUE(!status, "GET after DELETE should return false");

    return true;
}

bool test_delete_nonexistent_key(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    bool status = engine.del("nonexistent");
    ASSERT_TRUE(!status, "DELETE nonexistent key should return false");

    return true;
}

// Sequence number tests
bool test_sequence_numbers_memtable_priority(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "memtable_value");
    engine.flush();
    engine.put("key1", "newer_value");

    Entry result;
    engine.get("key1", result);
    ASSERT_EQ(result.value, "newer_value", "Memtable should take priority over SSTable");

    return true;
}

bool test_delete_then_put_sequence(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "value1");
    engine.del("key1");
    engine.put("key1", "value2");

    Entry result;
    bool status = engine.get("key1", result);
    ASSERT_TRUE(status, "Key should exist after PUT following DELETE");
    ASSERT_EQ(result.value, "value2", "Should return newest PUT value");

    return true;
}

bool test_multiple_sstables_newest_wins(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "value1");
    engine.flush();

    engine.put("key1", "value2");
    engine.flush();

    engine.put("key1", "value3");
    engine.flush();

    Entry result;
    engine.get("key1", result);
    ASSERT_EQ(result.value, "value3", "Newest SSTable should win");

    return true;
}

// WAL recovery tests
bool test_recovery_from_wal() {
    const std::string walPath = "data/log.bin";
    std::filesystem::remove_all("data");

    {
        StorageEngine engine(walPath);
        engine.put("key1", "value1");
        engine.put("key2", "value2");
        engine.del("key1");
    }

    {
        StorageEngine recoveredEngine(walPath);
        recoveredEngine.recover();

        Entry out;
        ASSERT_TRUE(!recoveredEngine.get("key1", out), "key1 should not exist after recovery");
        ASSERT_TRUE(recoveredEngine.get("key2", out), "key2 should exist after recovery");
        ASSERT_EQ(out.value, "value2", "Recovered value should match");
    }

    return true;
}

bool test_recovery_with_updates() {
    const std::string walPath = "data/log.bin";
    std::filesystem::remove_all("data");

    {
        StorageEngine engine(walPath);
        engine.put("key1", "old_value");
        engine.put("key1", "new_value");
    }

    {
        StorageEngine recoveredEngine(walPath);
        recoveredEngine.recover();

        Entry out;
        ASSERT_TRUE(recoveredEngine.get("key1", out), "key1 should exist");
        ASSERT_EQ(out.value, "new_value", "Should recover latest value");
    }

    return true;
}

// Flush and SSTable tests
bool test_flush_creates_sstable(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "value1");
    engine.put("key2", "value2");

    engine.flush();

    Entry result;
    ASSERT_TRUE(engine.get("key1", result), "key1 should exist after flush");
    ASSERT_EQ(result.value, "value1", "Value should be preserved");
    ASSERT_TRUE(engine.get("key2", result), "key2 should exist after flush");

    return true;
}

bool test_read_from_sstable_after_flush(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "value1");
    engine.flush();

    engine.put("dummy", "dummy");
    engine.del("dummy");

    Entry result;
    ASSERT_TRUE(engine.get("key1", result), "Should read from SSTable");
    ASSERT_EQ(result.value, "value1", "SSTable value should be correct");

    return true;
}

// Compaction tests
bool test_compaction_merges_sstables(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    for (int i = 0; i < 4; i++) {
        engine.put("key" + std::to_string(i), "value" + std::to_string(i));
        engine.flush();
    }

    for (int i = 0; i < 4; i++) {
        Entry result;
        std::string key = "key" + std::to_string(i);
        ASSERT_TRUE(engine.get(key, result), "Key should exist after compaction");
        ASSERT_EQ(result.value, "value" + std::to_string(i), "Value should be preserved");
    }

    return true;
}

bool test_compaction_removes_tombstones(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "value1");
    engine.flush();

    engine.put("key2", "value2");
    engine.flush();

    engine.del("key1");
    engine.flush();

    engine.put("key3", "value3");
    engine.flush();

    Entry result;
    ASSERT_TRUE(!engine.get("key1", result), "Deleted key should not exist");
    ASSERT_TRUE(engine.get("key2", result), "key2 should exist");
    ASSERT_TRUE(engine.get("key3", result), "key3 should exist");

    return true;
}

bool test_compaction_keeps_latest_version(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "version1");
    engine.flush();

    engine.put("key1", "version2");
    engine.flush();

    engine.put("key1", "version3");
    engine.flush();

    engine.put("dummy", "dummy");
    engine.flush();

    Entry result;
    engine.get("key1", result);
    ASSERT_EQ(result.value, "version3", "Should keep latest version after compaction");

    return true;
}

// Bloom filter tests
bool test_bloom_filter_negative_lookup(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    for (int i = 0; i < 100; i++) {
        engine.put("key" + std::to_string(i), "value" + std::to_string(i));
    }
    engine.flush();

    // Query non-existent keys (bloom filter should filter most)
    Entry result;
    for (int i = 100; i < 200; i++) {
        engine.get("nonexistent" + std::to_string(i), result);
        // No assertion - just testing that bloom filter works without crashes
    }

    return true;
}

// Spare index tests
bool test_large_dataset_with_index(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    const int NUM_KEYS = 1000;
    for (int i = 0; i < NUM_KEYS; i++) {
        std::string key = "key_" + std::string(5 - std::to_string(i).length(), '0') + std::to_string(i);
        engine.put(key, "value" + std::to_string(i));
    }
    engine.flush();

    Entry result;
    ASSERT_TRUE(engine.get("key_00000", result), "First key should exist");
    ASSERT_TRUE(engine.get("key_00500", result), "Middle key should exist");
    ASSERT_TRUE(engine.get("key_00999", result), "Last key should exist");

    return true;
}

// Stress tests
bool test_many_operations(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    const int NUM_OPS = 1000;

    for (int i = 0; i < NUM_OPS; i++) {
        engine.put("key" + std::to_string(i), "value" + std::to_string(i));
    }

    for (int i = 0; i < NUM_OPS; i++) {
        Entry result;
        std::string key = "key" + std::to_string(i);
        ASSERT_TRUE(engine.get(key, result), "Key should exist");
        ASSERT_EQ(result.value, "value" + std::to_string(i), "Value should match");
    }

    return true;
}

bool test_mixed_operations(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key1", "value1");
    engine.put("key2", "value2");
    engine.del("key1");
    engine.put("key3", "value3");
    engine.put("key1", "value1_new");
    engine.del("key2");

    Entry result;
    ASSERT_TRUE(engine.get("key1", result), "key1 should exist");
    ASSERT_EQ(result.value, "value1_new", "key1 should have new value");
    ASSERT_TRUE(!engine.get("key2", result), "key2 should be deleted");
    ASSERT_TRUE(engine.get("key3", result), "key3 should exist");

    return true;
}

// Edge Cases
bool test_empty_key(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("", "empty_key_value");
    Entry result;
    bool status = engine.get("", result);
    ASSERT_TRUE(status, "Empty key should be valid");
    ASSERT_EQ(result.value, "empty_key_value", "Empty key value should match");

    return true;
}

bool test_empty_value(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    engine.put("key_with_empty_value", "");
    Entry result;
    bool status = engine.get("key_with_empty_value", result);
    ASSERT_TRUE(status, "Key with empty value should exist");
    ASSERT_EQ(result.value, "", "Value should be empty string");

    return true;
}

bool test_large_value(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    std::string large_value(10000, 'x'); // 10KB value
    engine.put("large_key", large_value);

    Entry result;
    ASSERT_TRUE(engine.get("large_key", result), "Large value should be stored");
    ASSERT_EQ(result.value.size(), large_value.size(), "Large value size should match");

    return true;
}

// Persistence tests
bool test_persistence_across_restarts() {
    const std::string walPath = "data/log.bin";
    std::filesystem::remove_all("data");

    {
        StorageEngine engine(walPath);
        engine.put("persistent_key", "persistent_value");
        engine.flush();
    }

    {
        StorageEngine engine(walPath);
        Entry result;
        ASSERT_TRUE(engine.get("persistent_key", result), "Key should persist");
        ASSERT_EQ(result.value, "persistent_value", "Value should persist");
    }

    return true;
}

// run tests
void run_storage_engine_tests(TestFramework &framework) {
    StorageEngineTest fixture;
    std::cout << "Running Storage Engine Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    framework.run("test_simple_put_and_get", [&]() { return test_simple_put_and_get(fixture); });
    framework.run("test_update_existing_key", [&]() { return test_update_existing_key(fixture); });
    framework.run("test_get_nonexistent_key", [&]() { return test_get_nonexistent_key(fixture); });
    framework.run("test_simple_delete", [&]() { return test_simple_delete(fixture); });
    framework.run("test_delete_nonexistent_key", [&]() { return test_delete_nonexistent_key(fixture); });

    framework.run("test_sequence_numbers_memtable_priority", [&]() { return test_sequence_numbers_memtable_priority(fixture); });
    framework.run("test_delete_then_put_sequence", [&]() { return test_delete_then_put_sequence(fixture); });
    framework.run("test_multiple_sstables_newest_wins", [&]() { return test_multiple_sstables_newest_wins(fixture); });

    framework.run("test_recovery_from_wal", [&]() { return test_recovery_from_wal(); });
    framework.run("test_recovery_with_updates", [&]() { return test_recovery_with_updates(); });

    framework.run("test_flush_creates_sstable", [&]() { return test_flush_creates_sstable(fixture); });
    framework.run("test_read_from_sstable_after_flush", [&]() { return test_read_from_sstable_after_flush(fixture); });

    framework.run("test_compaction_merges_sstables", [&]() { return test_compaction_merges_sstables(fixture); });
    framework.run("test_compaction_removes_tombstones", [&]() { return test_compaction_removes_tombstones(fixture); });
    framework.run("test_compaction_keeps_latest_version", [&]() { return test_compaction_keeps_latest_version(fixture); });

    framework.run("test_bloom_filter_negative_lookup", [&]() { return test_bloom_filter_negative_lookup(fixture); });

    framework.run("test_large_dataset_with_index", [&]() { return test_large_dataset_with_index(fixture); });

    framework.run("test_many_operations", [&]() { return test_many_operations(fixture); });
    framework.run("test_mixed_operations", [&]() { return test_mixed_operations(fixture); });

    framework.run("test_empty_key", [&]() { return test_empty_key(fixture); });
    framework.run("test_empty_value", [&]() { return test_empty_value(fixture); });
    framework.run("test_large_value", [&]() { return test_large_value(fixture); });

    framework.run("test_persistence_across_restarts", [&]() { return test_persistence_across_restarts(); });

    std::cout << "========================================" << std::endl;
}
