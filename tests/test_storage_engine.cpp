#include "storage_engine.h"
#include "test_framework.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Test fixture class for storage engine tests
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

// Test cases
bool test_simple_put_and_get(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();
    bool status;
    std::string result;

    // Test initial put and get
    status = engine.put("user42", "123");

    ASSERT_TRUE(status, "PUT should succeed since adding a unique key");

    status = engine.get("user42", result);

    ASSERT_TRUE(status, "GET after PUT should succeed");
    ASSERT_EQ(result, "123", "Value should match what was put");

    return true;
}

bool test_update_existing_key(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    // Put initial value
    engine.put("user42", "123");

    // Update value
    engine.put("user42", "new123");

    std::string result;
    bool status = engine.get("user42", result);

    ASSERT_TRUE(status, "GET after update should succeed");
    ASSERT_EQ(result, "new123", "Value should be updated to new123");

    return true;
}

bool test_get_nonexistent_key(StorageEngineTest &fixture) {
    fixture.setUp();
    const auto &engine = fixture.getEngine();

    std::string result;
    bool status = engine.get("nonexistent", result);

    ASSERT_TRUE(!status, "GET on nonexistent key should return false");

    return true;
}

bool test_simple_delete(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();

    std::string result;
    engine.put("user42", "123");

    bool status = engine.del("user42");

    ASSERT_TRUE(status, "DELETE an existing key should work");
    return true;
}

bool test_memtable_persistence() {
    const std::string walPath = "data/log.bin";

    std::filesystem::remove(walPath);

    WriteAheadLog wal(walPath);
    MemTable memtable;

    memtable.put("key1", "value1");
    wal.append(Operation::PUT, "key1", "value1");

    memtable.put("key2", "value2");
    wal.append(Operation::PUT, "key2", "value2");

    memtable.del("key1");
    wal.append(Operation::DELETE, "key1", "");

    memtable.clear();

    MemTable recoveredMemtable;
    wal.replay([&recoveredMemtable](Operation op, const std::string &key, const std::string &value) {
        switch (op) {
        case Operation::PUT:
            recoveredMemtable.put(key, value);
            break;
        case Operation::DELETE:
            recoveredMemtable.del(key);
            break;
        }
    });

    std::string out;
    ASSERT_TRUE(!recoveredMemtable.get("key1", out), "key1 should be deleted");
    ASSERT_TRUE(recoveredMemtable.get("key2", out), "key2 should exist");
    ASSERT_EQ(out, "value2", "key2 value should match");

    std::filesystem::remove(walPath);

    return true;
}

// Main test runner
int main() {
    TestFramework framework("StorageEngine Tests");
    StorageEngineTest fixture;

    // Register and run tests
    framework.run("test_simple_put_and_get", [&]() { return test_simple_put_and_get(fixture); });

    framework.run("test_update_existing_key", [&]() { return test_update_existing_key(fixture); });

    framework.run("test_get_nonexistent_key", [&]() { return test_get_nonexistent_key(fixture); });

    framework.run("test_simple_delete", [&]() { return test_simple_delete(fixture); });

    framework.run("test_memtable_persistence", [&]() { return test_memtable_persistence(); });

    // Print summary and exit
    framework.printSummary();
    return framework.exitCode();
}
