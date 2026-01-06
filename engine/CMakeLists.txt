cmake_minimum_required(VERSION 3.20)
project(kv_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

include_directories(include)

find_library(ZLIB_LIB z)

set(CORE_SOURCES
    src/command_parser.cpp
    src/memtable.cpp
    src/sstable.cpp
    src/engine.cpp
    src/table_version.cpp
    src/wal.cpp
    src/test_framework.cpp
    src/bloom_filter.cpp
    src/lru_cache.cpp
    src/write_queue.cpp
)

add_library(core STATIC ${CORE_SOURCES})
target_link_libraries(core PUBLIC ${ZLIB_LIB})

add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE core)

file(GLOB TEST_SOURCES tests/*.cpp)
add_executable(tests_runner ${TEST_SOURCES})
target_link_libraries(tests_runner PRIVATE core)

file(GLOB BENCHMARK_SOURCES benchmarks/*.cpp)
foreach(bench_src ${BENCHMARK_SOURCES})
    get_filename_component(bench_name ${bench_src} NAME_WE)
    add_executable(${bench_name} ${bench_src})
    target_link_libraries(${bench_name} PRIVATE core)
endforeach()

add_custom_target(run COMMAND main DEPENDS main)
add_custom_target(test COMMAND tests_runner DEPENDS tests_runner)

add_custom_target(bench
    COMMAND benchmark_write_throughput
    COMMAND benchmark_read_latency
    COMMAND benchmark_bloom_filter
    COMMAND benchmark_compaction
    COMMAND benchmark_multithreaded
)

add_custom_target(check
    COMMAND cppcheck --enable=all --inconclusive --std=c++20
            --suppress=missingIncludeSystem
            --suppress=checkersReport
            --suppress=functionConst
            --suppress=normalCheckLevelMaxBranches
            --inline-suppr
            -I${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests
)
