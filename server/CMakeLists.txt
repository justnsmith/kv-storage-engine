cmake_minimum_required(VERSION 3.20)
project(kv_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

# Include engine headers
include_directories(${CMAKE_SOURCE_DIR}/../engine/include)
include_directories(${CMAKE_SOURCE_DIR}/include)

find_library(ZLIB_LIB z)

# Engine core sources (reuse from engine)
set(ENGINE_SOURCES
    ../engine/src/command_parser.cpp
    ../engine/src/memtable.cpp
    ../engine/src/sstable.cpp
    ../engine/src/engine.cpp
    ../engine/src/table_version.cpp
    ../engine/src/wal.cpp
    ../engine/src/test_framework.cpp
    ../engine/src/bloom_filter.cpp
    ../engine/src/lru_cache.cpp
    ../engine/src/write_queue.cpp
)

add_library(engine_core STATIC ${ENGINE_SOURCES})
target_link_libraries(engine_core PUBLIC ${ZLIB_LIB})

# Server executable
add_executable(kv_server
    src/main.cpp
    src/tcp_server.cpp
    src/connection_handler.cpp
)
target_link_libraries(kv_server PRIVATE engine_core)

# Add a run target
add_custom_target(run COMMAND kv_server DEPENDS kv_server)
