cmake_minimum_required(VERSION 3.10)
project(LSMTree VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable all warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find packages
find_package(nlohmann_json QUIET)
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
add_library(lsm_tree_naive STATIC ${NAIVE_SOURCES})
target_link_libraries(lsm_tree_naive PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(lsm_tree_naive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# CLI executable
add_executable(lsm_cli src/cli.cpp)
target_link_libraries(lsm_cli PRIVATE lsm_tree_naive)

# Add C++ test executable
add_executable(lsm_tree_test tests/cpp/main_test.cpp)
target_link_libraries(lsm_tree_test PRIVATE lsm_tree_naive)

# Create Python module
pybind11_add_module(lsm_tree_binding src/bindings/bindings.cpp)
target_link_libraries(lsm_tree_binding PRIVATE lsm_tree_naive)
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
set_target_properties(lsm_tree_naive lsm_tree_test lsm_tree_binding lsm_cli PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
) 