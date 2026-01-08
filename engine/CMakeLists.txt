cmake_minimum_required(VERSION 3.20)
project(kv_engine LANGUAGES CXX)

# -----------------------
# Language / build flags
# -----------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------
# Dependencies
# -----------------------
find_package(ZLIB REQUIRED)

# -----------------------
# Core engine library
# -----------------------
set(KV_ENGINE_CORE_SOURCES
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

add_library(kv_engine_core STATIC ${KV_ENGINE_CORE_SOURCES})

target_include_directories(kv_engine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(kv_engine_core
    PRIVATE
        -Wall -Wextra -Wpedantic
)

target_link_libraries(kv_engine_core
    PUBLIC
        ZLIB::ZLIB
)

# Canonical target name for consumers (server, future tools)
add_library(kv::engine ALIAS kv_engine_core)

# -----------------------
# Engine CLI executable
# -----------------------
add_executable(kv_engine
    src/main.cpp
)

target_link_libraries(kv_engine
    PRIVATE
        kv_engine_core
)

# -----------------------
# Tests
# -----------------------
enable_testing()

file(GLOB KV_ENGINE_TEST_SOURCES
    tests/*.cpp
)

add_executable(kv_engine_tests
    ${KV_ENGINE_TEST_SOURCES}
)

target_link_libraries(kv_engine_tests
    PRIVATE
        kv_engine_core
)

add_test(
    NAME kv_engine_tests
    COMMAND kv_engine_tests
)

# -----------------------
# Benchmarks
# -----------------------
file(GLOB KV_ENGINE_BENCHMARK_SOURCES
    benchmarks/*.cpp
)

set(KV_ENGINE_BENCH_TARGETS)

foreach(bench_src ${KV_ENGINE_BENCHMARK_SOURCES})
    get_filename_component(bench_name ${bench_src} NAME_WE)

    add_executable(${bench_name} ${bench_src})
    target_link_libraries(${bench_name} PRIVATE kv_engine_core)

    list(APPEND KV_ENGINE_BENCH_TARGETS ${bench_name})
endforeach()

add_custom_target(engine_bench
    DEPENDS ${KV_ENGINE_BENCH_TARGETS}
)

# -----------------------
# Convenience targets
# -----------------------
add_custom_target(engine_run
    COMMAND kv_engine
    DEPENDS kv_engine
)

add_custom_target(engine_check
    COMMAND cppcheck
        --enable=all
        --inconclusive
        --std=c++20
        --suppress=missingIncludeSystem
        --suppress=checkersReport
        --suppress=functionConst
        --suppress=normalCheckLevelMaxBranches
        --inline-suppr
        -I${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
