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

// Main test runner
int main() {
    TestFramework framework("StorageEngine Tests");
    StorageEngineTest fixture;

    // Register and run tests
    framework.run("test_simple_put_and_get", [&]() { return test_simple_put_and_get(fixture); });

    framework.run("test_update_existing_key", [&]() { return test_update_existing_key(fixture); });

    framework.run("test_get_nonexistent_key", [&]() { return test_get_nonexistent_key(fixture); });

    // Print summary and exit
    framework.printSummary();
    return framework.exitCode();
}
