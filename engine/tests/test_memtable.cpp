#include "memtable.h"
#include "test_framework.h"

class MemTableTest {
  public:
    MemTableTest() {
        setUp();
    }

    void setUp() {
        memtable_.clear();
    }

    MemTable &getMemTable() {
        return memtable_;
    }

  private:
    MemTable memtable_;
};

bool test_put_and_get(MemTableTest &fixture) {
    fixture.setUp();
    auto &mt = fixture.getMemTable();
    bool status;

    status = mt.put("key1", "value1", 1);

    ASSERT_TRUE(status, "PUT should succeed in memtable since unique key");

    Entry out;
    status = mt.get("key1", out);

    ASSERT_TRUE(status, "GET should succeed since value is in memtable");
    ASSERT_EQ(out.value, "value1", "Value should match");

    return true;
}

bool test_overwrite(MemTableTest &fixture) {
    fixture.setUp();
    auto &mt = fixture.getMemTable();
    bool status;

    status = mt.put("key1", "value1", 1);
    ASSERT_TRUE(status, "PUT should succeed");

    status = mt.put("key1", "value2", 2);
    ASSERT_TRUE(status, "PUT should succeed by updating current key with new value");

    Entry out;

    status = mt.get("key1", out);
    ASSERT_TRUE(status, "GET should succeed");
    ASSERT_EQ(out.value, "value2", "Value should match the updated value");

    return true;
}

bool test_delete(MemTableTest &fixture) {
    fixture.setUp();
    auto &mt = fixture.getMemTable();
    bool status;

    status = mt.put("key1", "value1", 1);
    ASSERT_TRUE(status, "PUT should succeed");

    status = mt.del("key1", 2);
    ASSERT_TRUE(status, "Delete should succeed");

    status = mt.del("key1", 3);
    ASSERT_TRUE(!status, "Delete should fail since key does not exist");
    return true;
}

bool test_clear(MemTableTest &fixture) {
    fixture.setUp();
    auto &mt = fixture.getMemTable();

    for (int i = 1; i <= 10; i++) {
        bool status;
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        status = mt.put(key, value, i);
        ASSERT_TRUE(status, "PUT should succeed");
    }

    mt.clear();
    ASSERT_EQ(mt.getSize(), 0, "MemTable size should be 0 after clear");
    return true;
}

bool test_snapshot(MemTableTest &fixture) {
    fixture.setUp();
    auto &mt = fixture.getMemTable();
    bool status;

    for (int i = 1; i <= 10; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        status = mt.put(key, value, i);
        ASSERT_TRUE(status, "PUT should succeed");
    }

    std::map<std::string, Entry> mtSnapshot = mt.snapshot();

    for (const auto &[k, v] : mtSnapshot) {
        Entry out;
        status = mt.get(k, out);
        ASSERT_TRUE(status, "GET should succeed");
        ASSERT_EQ(out.value, v.value, "Value in snapshot should match value in MemTable");
    }
    return true;
}

bool test_get_size(MemTableTest &fixture) {
    fixture.setUp();
    auto &mt = fixture.getMemTable();
    bool status;

    std::string key = "ab";
    std::string value = "cd";

    status = mt.put(key, value, 1);
    ASSERT_TRUE(status, "PUT should succeed");

    size_t expected = 4 + 2 + 2 + 1 + key.size() + value.size() + 8;

    ASSERT_EQ(mt.getSize(), expected, "Size calculation should match format");
    return true;
}

void run_memtable_tests(TestFramework &framework) {
    MemTableTest fixture;

    std::cout << "Running MemTable Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    framework.run("testput_and_get", [&]() { return test_put_and_get(fixture); });

    framework.run("test_update_existing_key", [&]() { return test_overwrite(fixture); });

    framework.run("test_delete", [&]() { return test_delete(fixture); });

    framework.run("test_clear", [&]() { return test_clear(fixture); });

    framework.run("test_snapshot", [&]() { return test_snapshot(fixture); });

    framework.run("test_get_size", [&]() { return test_get_size(fixture); });
}
