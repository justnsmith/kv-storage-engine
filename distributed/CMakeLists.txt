cmake_minimum_required(VERSION 3.20)
project(kv_distributed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(DISTRIBUTED_SOURCES
    replication/log_entry.cpp
    replication/log_shipper.cpp
    replication/leader.cpp
    replication/follower.cpp
    failure/heartbeat.cpp
    membership/node_registry.cpp
)

# Create static library
add_library(kv_distributed STATIC ${DISTRIBUTED_SOURCES})

target_include_directories(kv_distributed
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(kv_distributed
    PRIVATE
        -Wall -Wextra -Wpedantic
)

# Canonical target for consumers
add_library(kv::distributed ALIAS kv_distributed)
