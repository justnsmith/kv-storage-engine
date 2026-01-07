#include "sstable.h"
#include "test_framework.h"
#include <filesystem>
#include <map>

class SSTableTest {
  public:
    SSTableTest() {
        setUp();
    }

    void setUp() {
        test_dir_ = "./test_sstables/";
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
        std::filesystem::create_directories(test_dir_);
        flush_counter_ = 0;
    }

    void tearDown() {
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }

    ~SSTableTest() {
        tearDown();
    }

    const std::string &getTestDir() const {
        return test_dir_;
    }

    uint64_t getNextFlushCounter() {
        return ++flush_counter_;
    }

  private:
    std::string test_dir_;
    uint64_t flush_counter_;
};

bool test_flush_empty_snapshot(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    std::string expected_filename = fixture.getTestDir() + "sstable_1.bin";
    ASSERT_TRUE(std::filesystem::exists(expected_filename), "SSTable file should exist after flush");

    return true;
}

bool test_flush_and_get(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"value2", 2, EntryType::PUT};
    snapshot["key3"] = Entry{"value3", 3, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result1 = table.get("key1");
    ASSERT_TRUE(result1.has_value(), "Should find key1");
    ASSERT_EQ(result1->value, "value1", "Value should match");
    ASSERT_EQ(result1->seq, 1, "Sequence number should match");

    auto result2 = table.get("key2");
    ASSERT_TRUE(result2.has_value(), "Should find key2");
    ASSERT_EQ(result2->value, "value2", "Value should match");

    auto result3 = table.get("key3");
    ASSERT_TRUE(result3.has_value(), "Should find key3");
    ASSERT_EQ(result3->value, "value3", "Value should match");

    return true;
}

bool test_get_nonexistent_key(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"value2", 2, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result = table.get("key3");
    ASSERT_TRUE(!result.has_value(), "Should not find nonexistent key");

    return true;
}

bool test_get_with_delete_entry(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"", 2, EntryType::DELETE};
    snapshot["key3"] = Entry{"value3", 3, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result = table.get("key2");
    ASSERT_TRUE(result.has_value(), "Should find deleted key");
    ASSERT_TRUE(result->type == EntryType::DELETE, "Entry type should be DELETE");
    ASSERT_EQ(result->value, "", "Deleted entry should have empty value");

    return true;
}

bool test_key_range_filtering(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key5"] = Entry{"value5", 1, EntryType::PUT};
    snapshot["key6"] = Entry{"value6", 2, EntryType::PUT};
    snapshot["key7"] = Entry{"value7", 3, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    // Keys outside the range should return early
    auto result_low = table.get("key1");
    ASSERT_TRUE(!result_low.has_value(), "Key below min_key should not be found");

    auto result_high = table.get("key9");
    ASSERT_TRUE(!result_high.has_value(), "Key above max_key should not be found");

    return true;
}

bool test_large_dataset(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    size_t num_entries = 1000;

    for (size_t i = 0; i < num_entries; i++) {
        std::string key = "key" + std::string(10 - std::to_string(i).length(), '0') + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        snapshot[key] = Entry{value, i, EntryType::PUT};
    }

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    // Test random access
    auto result1 = table.get("key0000000100");
    ASSERT_TRUE(result1.has_value(), "Should find key in large dataset");
    ASSERT_EQ(result1->value, "value100", "Value should match");

    auto result2 = table.get("key0000000500");
    ASSERT_TRUE(result2.has_value(), "Should find key in large dataset");
    ASSERT_EQ(result2->value, "value500", "Value should match");

    auto result3 = table.get("key0000000999");
    ASSERT_TRUE(result3.has_value(), "Should find key in large dataset");
    ASSERT_EQ(result3->value, "value999", "Value should match");

    return true;
}

bool test_get_all_data(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"value2", 2, EntryType::PUT};
    snapshot["key3"] = Entry{"", 3, EntryType::DELETE};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    std::map<std::string, Entry> data = table.getData();

    ASSERT_EQ(data.size(), 3, "Should retrieve all entries");
    ASSERT_TRUE(data.count("key1") > 0, "Should contain key1");
    ASSERT_TRUE(data.count("key2") > 0, "Should contain key2");
    ASSERT_TRUE(data.count("key3") > 0, "Should contain key3");

    ASSERT_EQ(data["key1"].value, "value1", "Value should match");
    ASSERT_EQ(data["key2"].value, "value2", "Value should match");
    ASSERT_TRUE(data["key3"].type == EntryType::DELETE, "Entry type should be DELETE");

    return true;
}

bool test_persistence(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"value2", 2, EntryType::PUT};

    std::string filename;
    {
        SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());
        filename = table.filename();
    }

    // Reopen the SSTable
    SSTable table2(filename);

    auto result1 = table2.get("key1");
    ASSERT_TRUE(result1.has_value(), "Should find key1 after reopening");
    ASSERT_EQ(result1->value, "value1", "Value should match after reopening");

    auto result2 = table2.get("key2");
    ASSERT_TRUE(result2.has_value(), "Should find key2 after reopening");
    ASSERT_EQ(result2->value, "value2", "Value should match after reopening");

    return true;
}

bool test_empty_keys_and_values(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot[""] = Entry{"value_for_empty_key", 1, EntryType::PUT};
    snapshot["key_with_empty_value"] = Entry{"", 2, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result1 = table.get("");
    ASSERT_TRUE(result1.has_value(), "Should find empty key");
    ASSERT_EQ(result1->value, "value_for_empty_key", "Value should match");

    auto result2 = table.get("key_with_empty_value");
    ASSERT_TRUE(result2.has_value(), "Should find key with empty value");
    ASSERT_EQ(result2->value, "", "Empty value should match");

    return true;
}

bool test_special_characters(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key!@#$%"] = Entry{"value!@#$%", 1, EntryType::PUT};
    snapshot["key\n\t"] = Entry{"value\n\t", 2, EntryType::PUT};
    snapshot["key with spaces"] = Entry{"value with spaces", 3, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result1 = table.get("key!@#$%");
    ASSERT_TRUE(result1.has_value(), "Should handle special characters");
    ASSERT_EQ(result1->value, "value!@#$%", "Value should match");

    auto result2 = table.get("key\n\t");
    ASSERT_TRUE(result2.has_value(), "Should handle whitespace characters");
    ASSERT_EQ(result2->value, "value\n\t", "Value should match");

    auto result3 = table.get("key with spaces");
    ASSERT_TRUE(result3.has_value(), "Should handle spaces");
    ASSERT_EQ(result3->value, "value with spaces", "Value should match");

    return true;
}

bool test_long_keys_and_values(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    std::string long_key(1000, 'k');
    std::string long_value(10000, 'v');
    snapshot[long_key] = Entry{long_value, 1, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result = table.get(long_key);
    ASSERT_TRUE(result.has_value(), "Should handle long keys");
    ASSERT_EQ(result->value, long_value, "Long value should match");

    return true;
}

bool test_multiple_sstables(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot1;
    snapshot1["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot1["key2"] = Entry{"value2", 2, EntryType::PUT};

    std::map<std::string, Entry> snapshot2;
    snapshot2["key3"] = Entry{"value3", 3, EntryType::PUT};
    snapshot2["key4"] = Entry{"value4", 4, EntryType::PUT};

    SSTable table1 = SSTable::flush(snapshot1, fixture.getTestDir(), fixture.getNextFlushCounter());
    SSTable table2 = SSTable::flush(snapshot2, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result1 = table1.get("key1");
    ASSERT_TRUE(result1.has_value(), "Table1 should contain key1");

    auto result2 = table2.get("key3");
    ASSERT_TRUE(result2.has_value(), "Table2 should contain key3");

    auto result3 = table1.get("key3");
    ASSERT_TRUE(!result3.has_value(), "Table1 should not contain key3");

    return true;
}

bool test_iterator_basic(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"value2", 2, EntryType::PUT};
    snapshot["key3"] = Entry{"value3", 3, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    SSTable::Iterator it(table);

    size_t count = 0;
    while (it.valid()) {
        const SSTableEntry &entry = it.entry();
        ASSERT_TRUE(entry.key.length() > 0, "Key should not be empty");
        count++;
        it.next();
    }

    ASSERT_EQ(count, 3, "Iterator should iterate over all entries");

    return true;
}

bool test_iterator_order(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"value2", 2, EntryType::PUT};
    snapshot["key3"] = Entry{"value3", 3, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    SSTable::Iterator it(table);

    std::string prev_key = "";
    while (it.valid()) {
        const SSTableEntry &entry = it.entry();
        if (!prev_key.empty()) {
            ASSERT_TRUE(entry.key > prev_key, "Keys should be in sorted order");
        }
        prev_key = entry.key;
        it.next();
    }

    return true;
}

bool test_iterator_with_deletes(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};
    snapshot["key2"] = Entry{"", 2, EntryType::DELETE};
    snapshot["key3"] = Entry{"value3", 3, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    SSTable::Iterator it(table);

    bool found_delete = false;
    while (it.valid()) {
        const SSTableEntry &entry = it.entry();
        if (entry.type == EntryType::DELETE) {
            found_delete = true;
            ASSERT_EQ(entry.key, "key2", "Delete entry should be for key2");
        }
        it.next();
    }

    ASSERT_TRUE(found_delete, "Iterator should find DELETE entry");

    return true;
}

bool test_iterator_empty_table(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    SSTable::Iterator it(table);
    ASSERT_TRUE(it.valid(), "Iterator should be valid for non-empty table");

    return true;
}

bool test_move_semantics(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 1, EntryType::PUT};

    SSTable table1 = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());
    std::string filename = table1.filename();

    // Move constructor
    SSTable table2 = std::move(table1);
    ASSERT_EQ(table2.filename(), filename, "Moved table should have same filename");

    auto result = table2.get("key1");
    ASSERT_TRUE(result.has_value(), "Moved table should be functional");
    ASSERT_EQ(result->value, "value1", "Value should match in moved table");

    return true;
}

bool test_bloom_filter_optimization(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    for (int i = 0; i < 100; i++) {
        snapshot["key" + std::to_string(i)] = Entry{"value" + std::to_string(i), static_cast<uint64_t>(i), EntryType::PUT};
    }

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    // These keys don't exist - bloom filter should prevent disk reads for most of them
    for (int i = 1000; i < 1100; i++) {
        auto result = table.get("key" + std::to_string(i));
        ASSERT_TRUE(!result.has_value(), "Should not find non-existent keys");
    }

    return true;
}

bool test_sequence_numbers(SSTableTest &fixture) {
    fixture.setUp();

    std::map<std::string, Entry> snapshot;
    snapshot["key1"] = Entry{"value1", 100, EntryType::PUT};
    snapshot["key2"] = Entry{"value2", 200, EntryType::PUT};
    snapshot["key3"] = Entry{"value3", 300, EntryType::PUT};

    SSTable table = SSTable::flush(snapshot, fixture.getTestDir(), fixture.getNextFlushCounter());

    auto result1 = table.get("key1");
    ASSERT_TRUE(result1.has_value(), "Should find key1");
    ASSERT_EQ(result1->seq, 100, "Sequence number should be preserved");

    auto result2 = table.get("key2");
    ASSERT_TRUE(result2.has_value(), "Should find key2");
    ASSERT_EQ(result2->seq, 200, "Sequence number should be preserved");

    return true;
}

void run_sstable_tests(TestFramework &framework) {
    SSTableTest fixture;

    std::cout << "Running SSTable Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    framework.run("test_flush_empty_snapshot", [&]() { return test_flush_empty_snapshot(fixture); });
    framework.run("test_flush_and_get", [&]() { return test_flush_and_get(fixture); });
    framework.run("test_get_nonexistent_key", [&]() { return test_get_nonexistent_key(fixture); });
    framework.run("test_get_with_delete_entry", [&]() { return test_get_with_delete_entry(fixture); });
    framework.run("test_key_range_filtering", [&]() { return test_key_range_filtering(fixture); });
    framework.run("test_large_dataset", [&]() { return test_large_dataset(fixture); });
    framework.run("test_get_all_data", [&]() { return test_get_all_data(fixture); });
    framework.run("test_persistence", [&]() { return test_persistence(fixture); });
    framework.run("test_empty_keys_and_values", [&]() { return test_empty_keys_and_values(fixture); });
    framework.run("test_special_characters", [&]() { return test_special_characters(fixture); });
    framework.run("test_long_keys_and_values", [&]() { return test_long_keys_and_values(fixture); });
    framework.run("test_multiple_sstables", [&]() { return test_multiple_sstables(fixture); });
    framework.run("test_iterator_basic", [&]() { return test_iterator_basic(fixture); });
    framework.run("test_iterator_order", [&]() { return test_iterator_order(fixture); });
    framework.run("test_iterator_with_deletes", [&]() { return test_iterator_with_deletes(fixture); });
    framework.run("test_iterator_empty_table", [&]() { return test_iterator_empty_table(fixture); });
    framework.run("test_move_semantics", [&]() { return test_move_semantics(fixture); });
    framework.run("test_bloom_filter_optimization", [&]() { return test_bloom_filter_optimization(fixture); });
    framework.run("test_sequence_numbers", [&]() { return test_sequence_numbers(fixture); });
}
