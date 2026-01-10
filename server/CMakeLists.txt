cmake_minimum_required(VERSION 3.20)
project(kv_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

# Dependencies
find_package(ZLIB REQUIRED)

# Server executable
add_executable(kv_server
    src/main.cpp
    src/tcp_server.cpp
    src/connection_handler.cpp
    src/protocol.cpp
    src/config_parser.cpp
)

target_include_directories(kv_server
    PRIVATE
        include
        ${CMAKE_SOURCE_DIR}/distributed/include
)

target_link_libraries(kv_server
    PRIVATE
        kv::engine
        kv::distributed
        ZLIB::ZLIB
)

# Convenience target
add_custom_target(run
    COMMAND kv_server
    DEPENDS kv_server
)
