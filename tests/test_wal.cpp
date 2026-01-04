#include "test_framework.h"
#include "wal.h"
#include <filesystem>
#include <fstream>
#include <tuple>
#include <vector>

class WriteAheadLogTest {
  public:
    explicit WriteAheadLogTest(const std::string &path) : path_(path) {
        wal_ = std::make_unique<WriteAheadLog>(path_);
    }

    void setUp() {
        wal_.reset();

        std::filesystem::path p(path_);
        if (std::filesystem::exists(p)) {
            std::filesystem::remove(p);
        }
        std::filesystem::create_directories(p.parent_path());

        wal_ = std::make_unique<WriteAheadLog>(path_);
    }

    WriteAheadLog &wal() {
        return *wal_;
    }

    std::string &getPath() {
        return path_;
    }

    void restartWal() {
        wal_.reset();
        wal_ = std::make_unique<WriteAheadLog>(path_);
    }

  private:
    std::string path_;
    std::unique_ptr<WriteAheadLog> wal_;
};

bool test_single_append_and_replay(WriteAheadLogTest &fixture) {
    fixture.setUp();
    auto &wal = fixture.wal();

    wal.append(Operation::PUT, "key1", "value1", 1);
    wal.flush();

    std::vector<std::tuple<Operation, std::string, std::string>> applied;
    wal.replay([&](uint64_t, Operation op, std::string &key, std::string &value) { applied.emplace_back(op, key, value); });

    ASSERT_EQ(applied.size(), 1, "Replay should emit exactly one record");
    ASSERT_EQ(std::get<1>(applied[0]), "key1", "Key mismatch");
    ASSERT_EQ(std::get<2>(applied[0]), "value1", "Value mismatch");

    return true;
}

bool test_multiple_records_over_preserved(WriteAheadLogTest &fixture) {
    fixture.setUp();
    auto &wal = fixture.wal();

    wal.append(Operation::PUT, "a", "1", 1);
    wal.append(Operation::PUT, "b", "2", 2);
    wal.append(Operation::DELETE, "a", "", 3);
    wal.flush();

    std::vector<std::tuple<Operation, std::string, std::string>> applied;
    wal.replay([&](uint64_t, Operation op, std::string &key, std::string &value) { applied.emplace_back(op, key, value); });

    ASSERT_EQ(applied.size(), 3, "Should replay all appended records");
    ASSERT_EQ(std::get<1>(applied[1]), "b", "Second key incorrect");
    ASSERT_EQ(static_cast<int>(std::get<0>(applied[2])), static_cast<int>(Operation::DELETE), "Third op mismatch");

    return true;
}

bool test_replay_after_restart(WriteAheadLogTest &fixture) {
    fixture.setUp();

    {
        WriteAheadLog tempWal("data/log.bin");
        tempWal.append(Operation::PUT, "x", "10", 1);
        tempWal.append(Operation::PUT, "y", "20", 2);
        tempWal.flush();
    }

    std::vector<std::tuple<Operation, std::string, std::string>> applied;
    WriteAheadLog tempWal("data/log.bin");
    tempWal.replay([&](uint64_t, Operation op, std::string &key, std::string &value) { applied.emplace_back(op, key, value); });

    ASSERT_EQ(applied.size(), 2, "Restarted WAL should replay all records");
    ASSERT_EQ(std::get<1>(applied[1]), "y", "Recovered key mismatch");

    return true;
}

bool test_corruption_stops_replay(WriteAheadLogTest &fixture) {
    fixture.setUp();
    auto &wal = fixture.wal();
    wal.append(Operation::PUT, "key", "value", 1);
    wal.flush();

    fixture.restartWal();

    std::fstream file(fixture.getPath(), std::ios::in | std::ios::out | std::ios::binary);
    ASSERT_TRUE(file.is_open(), "Failed to open WAL file for corruption");

    // Seek past checksum (4 bytes) to corrupt the sequence number or Op
    file.seekp(4);
    char corrupt = 0xFF;
    file.write(&corrupt, 1);
    file.close();

    int appliedCount = 0;
    fixture.wal().replay([&](uint64_t, Operation, std::string &, std::string &) { appliedCount++; });

    ASSERT_EQ(appliedCount, 0, "Corrupted record must not be applied");

    return true;
}

bool test_truncated_record_not_applied(WriteAheadLogTest &fixture) {
    fixture.setUp();
    auto &wal = fixture.wal();
    wal.append(Operation::PUT, "key", "value", 1);
    wal.flush();

    fixture.restartWal();

    auto size = std::filesystem::file_size(fixture.getPath());
    std::filesystem::resize_file(fixture.getPath(), size - 2);

    int appliedCount = 0;
    fixture.wal().replay([&](uint64_t, Operation, std::string &, std::string &) { appliedCount++; });

    ASSERT_EQ(appliedCount, 0, "Partial record must not be applied");

    return true;
}

bool test_empty_wal(WriteAheadLogTest &fixture) {
    fixture.setUp();
    int appliedCount = 0;
    fixture.wal().replay([&](uint64_t, Operation, std::string &, std::string &) { appliedCount++; });

    ASSERT_EQ(appliedCount, 0, "Empty WAL should replay nothing");
    return true;
}

void run_wal_tests(TestFramework &framework) {
    WriteAheadLogTest fixture("data/log.bin");
    std::cout << "Running WAL Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    framework.run("test_single_append_and_replay", [&]() { return test_single_append_and_replay(fixture); });
    framework.run("test_multiple_records_over_preserved", [&]() { return test_multiple_records_over_preserved(fixture); });
    framework.run("test_replay_after_restart", [&]() { return test_replay_after_restart(fixture); });
    framework.run("test_corruption_stops_replay", [&]() { return test_corruption_stops_replay(fixture); });
    framework.run("test_truncated_record_not_applied", [&]() { return test_truncated_record_not_applied(fixture); });
    framework.run("test_empty_wal", [&]() { return test_empty_wal(fixture); });
    std::cout << "========================================" << std::endl;
}
