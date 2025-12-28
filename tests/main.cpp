#include "test_framework.h"

void run_storage_engine_tests(TestFramework &framework);
void run_memtable_tests(TestFramework &framework);
void run_wal_tests(TestFramework &framework);

int main() {
    TestFramework framework("All tests");

    // run_storage_engine_tests(framework);
    run_memtable_tests(framework);
    run_wal_tests(framework);

    framework.printSummary();
    return framework.exitCode();
}
