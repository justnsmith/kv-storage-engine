#include "test_framework.h"

void run_storage_engine_tests(TestFramework &framework);
void run_memtable_tests(TestFramework &framework);
void run_wal_tests(TestFramework &framework);
void run_bloom_filter_tests(TestFramework &framework);
void run_sstable_tests(TestFramework &framework);
void run_lru_cache_tests(TestFramework &framework);
void run_table_version_tests(TestFramework &framework);
void run_write_queue_tests(TestFramework &framework);

int main() {
    TestFramework framework("All tests");

    run_storage_engine_tests(framework);
    run_memtable_tests(framework);
    run_wal_tests(framework);
    run_bloom_filter_tests(framework);
    run_sstable_tests(framework);
    run_lru_cache_tests(framework);
    run_table_version_tests(framework);
    run_write_queue_tests(framework);

    framework.printSummary();
    return framework.exitCode();
}
