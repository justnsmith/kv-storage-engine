cmake_minimum_required(VERSION 3.20)
project(kv_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

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
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(core PUBLIC ZLIB::ZLIB)

add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE core)

enable_testing()

file(GLOB TEST_SOURCES tests/*.cpp)
add_executable(tests_runner ${TEST_SOURCES})
target_link_libraries(tests_runner PRIVATE core)

add_test(NAME kv_tests COMMAND tests_runner)

file(GLOB BENCHMARK_SOURCES benchmarks/*.cpp)
set(BENCH_TARGETS)

foreach(bench_src ${BENCHMARK_SOURCES})
    get_filename_component(bench_name ${bench_src} NAME_WE)
    add_executable(${bench_name} ${bench_src})
    target_link_libraries(${bench_name} PRIVATE core)
    list(APPEND BENCH_TARGETS ${bench_name})
endforeach()

add_custom_target(bench DEPENDS ${BENCH_TARGETS})

add_custom_target(run DEPENDS main COMMAND main)

add_custom_target(check
    COMMAND cppcheck --enable=all --inconclusive --std=c++20
            --suppress=missingIncludeSystem
            --suppress=checkersReport
            --suppress=functionConst
            --suppress=normalCheckLevelMaxBranches
            --inline-suppr
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
