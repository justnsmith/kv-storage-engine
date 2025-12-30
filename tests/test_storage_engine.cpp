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

// Test cases
bool test_simple_put_and_get(StorageEngineTest &fixture) {
    fixture.setUp();
    auto &engine = fixture.getEngine();
    bool status;
    Entry result;

    status = engine.put("user42", "123");

    ASSERT_TRUE(status, "PUT should succeed since adding a unique key");

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
    ASSERT_EQ(result.value, "new123", "Value should be updated to new123");

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

    ASSERT_TRUE(status, "DELETE an existing key should work");
    return true;
}

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
        ASSERT_EQ(out.value, "value2", "Recovered value should match last written value");
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

    framework.run("test_recovery_from_wal", [&]() { return test_recovery_from_wal(); });
    std::cout << "========================================" << std::endl;
}
