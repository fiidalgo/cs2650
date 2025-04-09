# CMake minimum required version
cmake_minimum_required(VERSION 3.14)

# Project name and version
project(LSM_Tree_DB VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

# Create include directory for server if it doesn't exist
file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/include/server)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# Define sources for naive implementation
set(NAIVE_SOURCES
    src/naive/memtable.cpp
    src/naive/sstable.cpp
    src/naive/lsm_tree.cpp
    src/naive/manifest.cpp
)

# Create the naive implementation library
add_library(lsm_naive STATIC ${NAIVE_SOURCES})

# Server implementation sources
set(SERVER_SOURCES
    src/server/server.cpp
    src/server/dsl_parser.cpp
)

# Create the server library
add_library(lsm_server STATIC ${SERVER_SOURCES})
target_link_libraries(lsm_server lsm_naive)

# Create the server executable
add_executable(server src/server/main.cpp)
target_link_libraries(server lsm_server)

# Create the client executable
add_executable(client src/client/client.cpp)
target_link_libraries(client lsm_server)

# Tests section
enable_testing()

# MemTable test
add_executable(test_memtable tests/functional/test_memtable.cpp)
target_link_libraries(test_memtable lsm_naive)
add_test(NAME MemTableTest COMMAND test_memtable)

# Installation rules
install(TARGETS server client
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}") 