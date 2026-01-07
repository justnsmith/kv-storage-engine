#include "table_version.h"
#include "test_framework.h"
#include <filesystem>
#include <map>

class TableVersionTest {
  public:
    TableVersionTest() {
        setUp();
    }

    void setUp() {
        test_dir_ = "./test_table_version/";
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

    ~TableVersionTest() {
        tearDown();
    }

    const std::string &getTestDir() const {
        return test_dir_;
    }

    uint64_t getNextFlushCounter() {
        return ++flush_counter_;
    }

    std::shared_ptr<SSTable> createTestSSTable(uint64_t id) {
        std::map<std::string, Entry> snapshot;
        snapshot["key" + std::to_string(id)] = Entry{"value" + std::to_string(id), id, EntryType::PUT};

        SSTable table = SSTable::flush(snapshot, test_dir_, id);
        return std::make_shared<SSTable>(std::move(table));
    }

  private:
    std::string test_dir_;
    uint64_t flush_counter_;
};

bool test_table_version_creation(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    ASSERT_EQ(version->version_number, 0, "Initial version number should be 0");
    ASSERT_EQ(version->flush_counter, 0, "Initial flush counter should be 0");
    ASSERT_EQ(version->levels.size(), 0, "Initial levels should be empty");
    ASSERT_EQ(version->sstables.size(), 0, "Initial sstables should be empty");

    return true;
}

bool test_add_sstable_to_level_0(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();
    auto sst = fixture.createTestSSTable(1);

    SSTableMeta meta{1, "key1", "key1", 100, 500, 0};
    version->addSSTable(sst, meta);

    ASSERT_EQ(version->sstables.size(), 1, "Should have 1 sstable");
    ASSERT_EQ(version->levels.size(), 1, "Should have 1 level");
    ASSERT_EQ(version->levels[0].size(), 1, "Level 0 should have 1 sstable");
    ASSERT_EQ(version->levels[0][0].id, 1, "SSTable ID should match");
    ASSERT_EQ(version->levels[0][0].level, 0, "Level should be 0");

    return true;
}

bool test_add_sstable_to_different_levels(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    auto sst2 = fixture.createTestSSTable(2);
    auto sst3 = fixture.createTestSSTable(3);

    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    SSTableMeta meta2{2, "key2", "key2", 200, 600, 1};
    SSTableMeta meta3{3, "key3", "key3", 300, 700, 2};

    version->addSSTable(sst1, meta1);
    version->addSSTable(sst2, meta2);
    version->addSSTable(sst3, meta3);

    ASSERT_EQ(version->sstables.size(), 3, "Should have 3 sstables");
    ASSERT_EQ(version->levels.size(), 3, "Should have 3 levels");
    ASSERT_EQ(version->levels[0].size(), 1, "Level 0 should have 1 sstable");
    ASSERT_EQ(version->levels[1].size(), 1, "Level 1 should have 1 sstable");
    ASSERT_EQ(version->levels[2].size(), 1, "Level 2 should have 1 sstable");

    return true;
}

bool test_add_multiple_sstables_same_level(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    auto sst2 = fixture.createTestSSTable(2);
    auto sst3 = fixture.createTestSSTable(3);

    SSTableMeta meta1{1, "key1", "key3", 100, 500, 1};
    SSTableMeta meta2{2, "key4", "key6", 200, 600, 1};
    SSTableMeta meta3{3, "key7", "key9", 300, 700, 1};

    version->addSSTable(sst1, meta1);
    version->addSSTable(sst2, meta2);
    version->addSSTable(sst3, meta3);

    ASSERT_EQ(version->sstables.size(), 3, "Should have 3 sstables");
    ASSERT_EQ(version->levels.size(), 2, "Should have 2 levels (0 is empty, 1 has tables)");
    ASSERT_EQ(version->levels[1].size(), 3, "Level 1 should have 3 sstables");

    return true;
}

bool test_find_sstable_by_id(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    auto sst2 = fixture.createTestSSTable(2);
    auto sst3 = fixture.createTestSSTable(3);

    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    SSTableMeta meta2{2, "key2", "key2", 200, 600, 0};
    SSTableMeta meta3{3, "key3", "key3", 300, 700, 1};

    version->addSSTable(sst1, meta1);
    version->addSSTable(sst2, meta2);
    version->addSSTable(sst3, meta3);

    auto found1 = version->findSSTableById(1);
    ASSERT_TRUE(found1 != nullptr, "Should find sstable with ID 1");

    auto found2 = version->findSSTableById(2);
    ASSERT_TRUE(found2 != nullptr, "Should find sstable with ID 2");

    auto found3 = version->findSSTableById(3);
    ASSERT_TRUE(found3 != nullptr, "Should find sstable with ID 3");

    return true;
}

bool test_find_nonexistent_sstable(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    version->addSSTable(sst1, meta1);

    auto found = version->findSSTableById(999);
    ASSERT_TRUE(found == nullptr, "Should not find nonexistent sstable");

    return true;
}

bool test_remove_sstables_by_ids(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    auto sst2 = fixture.createTestSSTable(2);
    auto sst3 = fixture.createTestSSTable(3);

    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    SSTableMeta meta2{2, "key2", "key2", 200, 600, 0};
    SSTableMeta meta3{3, "key3", "key3", 300, 700, 1};

    version->addSSTable(sst1, meta1);
    version->addSSTable(sst2, meta2);
    version->addSSTable(sst3, meta3);

    ASSERT_EQ(version->sstables.size(), 3, "Should have 3 sstables before removal");

    std::vector<uint64_t> ids_to_remove = {1, 3};
    version->removeSSTablesByIds(ids_to_remove);

    ASSERT_EQ(version->sstables.size(), 1, "Should have 1 sstable after removal");
    ASSERT_EQ(version->levels[0].size(), 1, "Level 0 should have 1 sstable");
    ASSERT_EQ(version->levels[1].size(), 0, "Level 1 should be empty");

    auto found2 = version->findSSTableById(2);
    ASSERT_TRUE(found2 != nullptr, "SSTable 2 should still exist");

    auto found1 = version->findSSTableById(1);
    ASSERT_TRUE(found1 == nullptr, "SSTable 1 should be removed");

    auto found3 = version->findSSTableById(3);
    ASSERT_TRUE(found3 == nullptr, "SSTable 3 should be removed");

    return true;
}

bool test_remove_all_sstables_from_level(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    auto sst2 = fixture.createTestSSTable(2);

    SSTableMeta meta1{1, "key1", "key1", 100, 500, 1};
    SSTableMeta meta2{2, "key2", "key2", 200, 600, 1};

    version->addSSTable(sst1, meta1);
    version->addSSTable(sst2, meta2);

    std::vector<uint64_t> ids_to_remove = {1, 2};
    version->removeSSTablesByIds(ids_to_remove);

    ASSERT_EQ(version->sstables.size(), 0, "Should have no sstables");
    ASSERT_EQ(version->levels[1].size(), 0, "Level 1 should be empty");

    return true;
}

bool test_remove_nonexistent_ids(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    version->addSSTable(sst1, meta1);

    size_t original_size = version->sstables.size();

    std::vector<uint64_t> ids_to_remove = {999, 888};
    version->removeSSTablesByIds(ids_to_remove);

    ASSERT_EQ(version->sstables.size(), original_size, "Size should remain unchanged");

    return true;
}

bool test_copy_from(TableVersionTest &fixture) {
    fixture.setUp();

    auto original = std::make_shared<TableVersion>();
    original->version_number = 5;
    original->flush_counter = 10;

    auto sst1 = fixture.createTestSSTable(1);
    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    original->addSSTable(sst1, meta1);

    auto copied = TableVersion::copyFrom(original);

    ASSERT_EQ(copied->version_number, 6, "Version number should be incremented");
    ASSERT_EQ(copied->flush_counter, 10, "Flush counter should be copied");
    ASSERT_EQ(copied->sstables.size(), 1, "SSTables should be copied");
    ASSERT_EQ(copied->levels.size(), 1, "Levels should be copied");
    ASSERT_EQ(copied->levels[0].size(), 1, "Level 0 metadata should be copied");

    return true;
}

bool test_copy_from_empty_version(TableVersionTest &fixture) {
    fixture.setUp();

    auto original = std::make_shared<TableVersion>();
    auto copied = TableVersion::copyFrom(original);

    ASSERT_EQ(copied->version_number, 1, "Version number should be incremented from 0");
    ASSERT_EQ(copied->flush_counter, 0, "Flush counter should be 0");
    ASSERT_EQ(copied->sstables.size(), 0, "Should have no sstables");
    ASSERT_EQ(copied->levels.size(), 0, "Should have no levels");

    return true;
}

bool test_copy_from_nullptr(TableVersionTest &fixture) {
    fixture.setUp();

    auto copied = TableVersion::copyFrom(nullptr);

    ASSERT_TRUE(copied != nullptr, "Should create new version even with nullptr");
    ASSERT_EQ(copied->version_number, 0, "Version number should be 0");
    ASSERT_EQ(copied->sstables.size(), 0, "Should have no sstables");

    return true;
}

bool test_copy_independence(TableVersionTest &fixture) {
    fixture.setUp();

    auto original = std::make_shared<TableVersion>();
    auto sst1 = fixture.createTestSSTable(1);
    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    original->addSSTable(sst1, meta1);

    auto copied = TableVersion::copyFrom(original);

    ASSERT_EQ(copied->sstables.size(), 1, "Copied should have 1 sstable initially");

    // Modify original
    auto sst2 = fixture.createTestSSTable(2);
    SSTableMeta meta2{2, "key2", "key2", 200, 600, 0};
    original->addSSTable(sst2, meta2);

    ASSERT_EQ(original->sstables.size(), 2, "Original should have 2 sstables");
    ASSERT_EQ(copied->sstables.size(), 1, "Copied should still have 1 sstable (independent copy)");

    // Verify the copied version wasn't affected
    auto found1 = copied->findSSTableById(1);
    ASSERT_TRUE(found1 != nullptr, "Copied version should still have sstable 1");

    auto found2 = copied->findSSTableById(2);
    ASSERT_TRUE(found2 == nullptr, "Copied version should not have sstable 2");

    return true;
}

bool test_version_manager_creation(TableVersionTest &fixture) {
    fixture.setUp();

    VersionManager manager;
    auto version = manager.getCurrentVersion();

    ASSERT_TRUE(version != nullptr, "Should have a current version");
    ASSERT_EQ(version->version_number, 0, "Initial version number should be 0");

    return true;
}

bool test_version_manager_install_version(TableVersionTest &fixture) {
    fixture.setUp();

    VersionManager manager;

    auto new_version = std::make_shared<TableVersion>();
    new_version->version_number = 5;
    new_version->flush_counter = 10;

    manager.installVersion(new_version);

    auto current = manager.getCurrentVersion();
    ASSERT_EQ(current->version_number, 5, "Version number should match installed version");
    ASSERT_EQ(current->flush_counter, 10, "Flush counter should match installed version");

    return true;
}

bool test_version_manager_get_version_for_modification(TableVersionTest &fixture) {
    fixture.setUp();

    VersionManager manager;
    auto current = manager.getCurrentVersion();
    current->version_number = 3;
    current->flush_counter = 7;

    auto for_modification = manager.getVersionForModification();

    ASSERT_EQ(for_modification->version_number, 4, "Version should be incremented");
    ASSERT_EQ(for_modification->flush_counter, 7, "Flush counter should be preserved");

    // Original should be unchanged
    auto still_current = manager.getCurrentVersion();
    ASSERT_EQ(still_current->version_number, 3, "Original version should be unchanged");

    return true;
}

bool test_version_manager_multiple_modifications(TableVersionTest &fixture) {
    fixture.setUp();

    VersionManager manager;

    // Get version for modification
    auto v1 = manager.getVersionForModification();
    auto sst1 = fixture.createTestSSTable(1);
    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    v1->addSSTable(sst1, meta1);

    // Install it
    manager.installVersion(v1);

    // Get another version for modification
    auto v2 = manager.getVersionForModification();
    auto sst2 = fixture.createTestSSTable(2);
    SSTableMeta meta2{2, "key2", "key2", 200, 600, 0};
    v2->addSSTable(sst2, meta2);

    // Install it
    manager.installVersion(v2);

    auto final = manager.getCurrentVersion();
    ASSERT_EQ(final->sstables.size(), 2, "Should have accumulated sstables");
    ASSERT_TRUE(final->version_number > 0, "Version number should have increased");

    return true;
}

bool test_metadata_preservation(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    SSTableMeta meta1{1, "aaa", "ccc", 100, 500, 0};
    version->addSSTable(sst1, meta1);

    ASSERT_EQ(version->levels[0][0].minKey, "aaa", "Min key should be preserved");
    ASSERT_EQ(version->levels[0][0].maxKey, "ccc", "Max key should be preserved");
    ASSERT_EQ(version->levels[0][0].maxSeq, 100, "Max sequence should be preserved");
    ASSERT_EQ(version->levels[0][0].sizeBytes, 500, "Size should be preserved");

    return true;
}

bool test_level_resizing(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    // Add to level 5 directly (should resize levels vector)
    auto sst1 = fixture.createTestSSTable(1);
    SSTableMeta meta1{1, "key1", "key1", 100, 500, 5};
    version->addSSTable(sst1, meta1);

    ASSERT_TRUE(version->levels.size() >= 6, "Levels should be resized to accommodate level 5");
    ASSERT_EQ(version->levels[5].size(), 1, "Level 5 should have 1 sstable");

    return true;
}

bool test_empty_levels_between(TableVersionTest &fixture) {
    fixture.setUp();

    auto version = std::make_shared<TableVersion>();

    auto sst1 = fixture.createTestSSTable(1);
    auto sst2 = fixture.createTestSSTable(2);

    SSTableMeta meta1{1, "key1", "key1", 100, 500, 0};
    SSTableMeta meta2{2, "key2", "key2", 200, 600, 3};

    version->addSSTable(sst1, meta1);
    version->addSSTable(sst2, meta2);

    ASSERT_EQ(version->levels.size(), 4, "Should have 4 levels (0-3)");
    ASSERT_EQ(version->levels[0].size(), 1, "Level 0 should have 1 sstable");
    ASSERT_EQ(version->levels[1].size(), 0, "Level 1 should be empty");
    ASSERT_EQ(version->levels[2].size(), 0, "Level 2 should be empty");
    ASSERT_EQ(version->levels[3].size(), 1, "Level 3 should have 1 sstable");

    return true;
}

void run_table_version_tests(TestFramework &framework) {
    TableVersionTest fixture;

    std::cout << "Running Table Version Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    framework.run("test_table_version_creation", [&]() { return test_table_version_creation(fixture); });
    framework.run("test_add_sstable_to_level_0", [&]() { return test_add_sstable_to_level_0(fixture); });
    framework.run("test_add_sstable_to_different_levels", [&]() { return test_add_sstable_to_different_levels(fixture); });
    framework.run("test_add_multiple_sstables_same_level", [&]() { return test_add_multiple_sstables_same_level(fixture); });
    framework.run("test_find_sstable_by_id", [&]() { return test_find_sstable_by_id(fixture); });
    framework.run("test_find_nonexistent_sstable", [&]() { return test_find_nonexistent_sstable(fixture); });
    framework.run("test_remove_sstables_by_ids", [&]() { return test_remove_sstables_by_ids(fixture); });
    framework.run("test_remove_all_sstables_from_level", [&]() { return test_remove_all_sstables_from_level(fixture); });
    framework.run("test_remove_nonexistent_ids", [&]() { return test_remove_nonexistent_ids(fixture); });
    framework.run("test_copy_from", [&]() { return test_copy_from(fixture); });
    framework.run("test_copy_from_empty_version", [&]() { return test_copy_from_empty_version(fixture); });
    framework.run("test_copy_from_nullptr", [&]() { return test_copy_from_nullptr(fixture); });
    framework.run("test_copy_independence", [&]() { return test_copy_independence(fixture); });
    framework.run("test_version_manager_creation", [&]() { return test_version_manager_creation(fixture); });
    framework.run("test_version_manager_install_version", [&]() { return test_version_manager_install_version(fixture); });
    framework.run("test_version_manager_get_version_for_modification",
                  [&]() { return test_version_manager_get_version_for_modification(fixture); });
    framework.run("test_version_manager_multiple_modifications", [&]() { return test_version_manager_multiple_modifications(fixture); });
    framework.run("test_metadata_preservation", [&]() { return test_metadata_preservation(fixture); });
    framework.run("test_level_resizing", [&]() { return test_level_resizing(fixture); });
    framework.run("test_empty_levels_between", [&]() { return test_empty_levels_between(fixture); });
}
