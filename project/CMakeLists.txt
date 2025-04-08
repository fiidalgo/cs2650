cmake_minimum_required(VERSION 3.10)
project(LSMTree VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Add compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Enable all warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find packages
find_package(nlohmann_json REQUIRED)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Find Python and pybind11
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG QUIET)

if(NOT pybind11_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.10.4
    )
    FetchContent_MakeAvailable(pybind11)
endif()

# Add source files
set(NAIVE_SOURCES
    src/naive/memtable.cpp
    src/naive/sstable.cpp
    src/naive/lsm_tree.cpp
)

# Add library for naive implementation
add_library(naive_lsm STATIC ${NAIVE_SOURCES})
target_link_libraries(naive_lsm PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(naive_lsm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Compaction implementation
add_library(compaction_lsm STATIC
    src/naive/memtable.cpp
    src/naive/sstable.cpp
    src/compaction/lsm_tree.cpp
)
target_link_libraries(compaction_lsm PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(compaction_lsm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# CLI executable
add_executable(lsm_cli src/cli.cpp)
target_link_libraries(lsm_cli PRIVATE naive_lsm)

# Testing
find_package(GTest REQUIRED)
enable_testing()

# Compaction tuning tests
add_executable(compaction_tuning_tests tests/cpp/compaction_tuning_test.cpp)
target_link_libraries(compaction_tuning_tests compaction_lsm GTest::GTest GTest::Main)

# Set RPATH for the executable to find GTest libraries
set_target_properties(compaction_tuning_tests PROPERTIES 
    INSTALL_RPATH "/Users/nico/miniconda3/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
    SKIP_BUILD_RPATH FALSE
)

# Force the linker to include the GTest libraries
if(APPLE)
    set_target_properties(compaction_tuning_tests PROPERTIES
        LINK_FLAGS "-Wl,-rpath,/Users/nico/miniconda3/lib"
    )
endif()

add_test(NAME compaction_tuning_tests COMMAND compaction_tuning_tests)

# Create data directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data/naive)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data/compaction)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data/bloom)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data/fence)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../data/concurrency)

# Create Python module
pybind11_add_module(lsm_tree_binding src/bindings/bindings.cpp)
target_link_libraries(lsm_tree_binding PRIVATE naive_lsm)
target_include_directories(lsm_tree_binding PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Installation rules
install(TARGETS lsm_tree_binding
        LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS lsm_cli
        RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Add build instructions to README
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/BUILD.md
"# Build Instructions

## Requirements
- CMake 3.10 or higher
- C++17 compatible compiler
- Python 3.6 or higher with development headers
- pybind11 (will be downloaded automatically if not found)
- nlohmann_json (will be downloaded automatically if not found)

## Building

```bash
# Create a build directory
mkdir -p build
cd build

# Configure CMake
cmake ..

# Build
cmake --build .

# Install the Python module and CLI locally
cmake --install .

# Go back to the project root
cd ..
```

## Running the CLI

```bash
# Run the CLI with default settings (naive implementation)
./bin/lsm_cli

# Run with a specific implementation
./bin/lsm_cli --implementation naive

# For help
./bin/lsm_cli --help
```

## Running Tests

After building, you can run the C++ tests:

```bash
./build/lsm_tree_test
```

And the Python tests:

```bash
python tests/python/test_runner.py
```
")

# Add a simple shell script to build the project
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/build.sh
"#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure and build
cmake ..
cmake --build .

# Install the Python module and CLI locally
cmake --install .

echo \"Build completed successfully!\"
echo \"Run the CLI with: ./bin/lsm_cli\"
echo \"Run the C++ test with: ./build/lsm_tree_test\"
echo \"Run the Python test with: python tests/python/test_runner.py\"
")

# Make the shell script executable
file(CHMOD ${CMAKE_CURRENT_SOURCE_DIR}/build.sh
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Add .gitignore
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/.gitignore
"# Build files
/build/

# Python cache
__pycache__/
*.py[cod]
*$py.class

# Data files
/project/data/

# Editor files
.vscode/
.idea/
*.swp
*.swo

# System files
.DS_Store
Thumbs.db
")

# Set output directories
set_target_properties(naive_lsm compaction_lsm lsm_tree_binding lsm_cli compaction_tuning_tests PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
) 