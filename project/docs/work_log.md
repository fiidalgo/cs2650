# Work Log

This document tracks the development progress of this project. It includes details on progress, implementation decisions, and future plans.

## 2025-04-08

### Completed

- Cleaned up the repository structure and set up the project organization
- Created a detailed project plan outlining the step-by-step implementation approach
- Designed the overall architecture as a client-server system
- Defined the project's progression through phases: naive implementation → compaction → bloom filters → fence pointers → concurrency
- Established the DSL for interacting with the database
- Designed the testing framework and metrics collection approach

### Design Decisions

#### LSM-Tree Architecture

The LSM-Tree (Log-Structured Merge Tree) design was chosen because it offers excellent write performance while maintaining reasonable read performance.

1. **MemTable**: Using a balanced tree (`std::map`) rather than a skip list for the initial implementation. This simplifies development while still providing $O(\log n)$ operations. Skip lists could be considered later for potential performance improvements.

2. **SSTable Format**: Designed with a simple format initially (keys sorted, values adjacent) to facilitate easier debugging and implementation. The format will evolve as optimizations are added.

3. **Merge Policy**: Selected leveling as the default compaction strategy (over tiering) because:

   - Provides better read performance due to fewer SSTables to search
   - Simpler to reason about initially
   - More widely used in production systems (e.g., LevelDB, RocksDB)

4. **Client-Server Architecture**: The two-process design (separate server and client) provides:
   - Clean separation of concerns between storage engine and user interface
   - Support for network-based access and multiple clients
   - More realistic deployment structure that mirrors production database systems

#### Testing Framework Approach

Designed a comprehensive testing framework to:

- Generate various workloads (read-heavy, write-heavy, range-heavy, mixed)
- Collect detailed metrics (latency, throughput, I/O statistics)
- Support hyperparameter tuning with grid search
- Visualize performance trends

This approach will allow data-driven optimization decisions rather than guessing.

### Optimization Explanations

1. **Compaction**: Process of merging multiple SSTables to control their count and organization. Key to maintaining read performance as the dataset grows.

   - Leveling: Organizes SSTables into levels; each level is K times larger than the one above
   - Tiering: Groups SSTables into tiers; compaction happens when a tier fills up

2. **Bloom Filters**: Probabilistic data structures that can quickly tell if a key is definitely not in a set. In LSM-Trees, they allow skipping SSTables that don't contain a queried key, dramatically improving read performance.

3. **Fence Pointers**: Index structures that divide SSTables into blocks and store the minimum key for each block. Allow binary search to quickly locate the correct block for a key, avoiding scanning the entire SSTable.

4. **Concurrency Handling**: Techniques to allow multiple threads to safely access the LSM-Tree simultaneously, improving throughput on multi-core systems.

### Next Steps

1. Set up the basic project structure (directories, CMakeLists.txt)
2. Implement the core MemTable data structure
3. Implement the SSTable format and I/O operations
4. Create the naive LSM-Tree implementation (put, get, range, delete operations)
5. Begin work on the server component to handle client connections

## 2025-04-09

### Completed

1. **Created the CMakeLists.txt build configuration file**
   - Configured the project to use C++17 standard (required for modern features like `std::optional`)
   - Set up the build structure for all project components
   - Defined library targets for the LSM-Tree components
   - Configured executable targets for server, client, and tests
   - Added compiler warning flags for better code quality
   - Organized output directories for a clean build structure

### Design Decisions

1. **Modular Build Structure**

   - Created separate static libraries for different components:
     - `lsm_naive`: Core naive LSM-Tree implementation
     - `lsm_server`: Server component that depends on the LSM-Tree
   - Benefits: Clearer dependencies, faster incremental builds, better testability

2. **Compiler and Language Settings**

   - Chose C++17 for modern language features needed by the project
   - Enabled strict compiler warnings (`-Wall -Wextra -Werror`)
   - Configured debug symbols for development builds
   - Set high optimization level for release builds

3. **Output Organization**
   - Structured outputs to keep the build directory clean:
     - Executables go to `bin/` directory
     - Libraries go to `lib/` directory
   - Avoids cluttering the source directory with build artifacts

### CMake Specifics and Structure

The CMakeLists.txt file defines the build process in several stages:

1. **Project Configuration**

   ```cmake
   cmake_minimum_required(VERSION 3.14)
   project(LSM_Tree_DB VERSION 0.1.0 LANGUAGES CXX)
   set(CMAKE_CXX_STANDARD 17)
   ```

   Defines basic project properties, required CMake version, and language standard.

2. **Compiler and Output Settings**

   ```cmake
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
   set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
   set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
   ```

   Configures compiler behavior and organizes output files.

3. **Component Definitions**

   ```cmake
   set(NAIVE_SOURCES
       src/naive/memtable.cpp
       src/naive/sstable.cpp
       src/naive/lsm_tree.cpp
       src/naive/manifest.cpp
   )
   add_library(lsm_naive STATIC ${NAIVE_SOURCES})
   ```

   Defines source files for each component and creates library targets.

4. **Executable Targets**

   ```cmake
   add_executable(server src/server/main.cpp)
   target_link_libraries(server lsm_server)
   ```

   Creates executable targets and links them with required libraries.

5. **Testing Configuration**
   ```cmake
   enable_testing()
   add_executable(test_memtable tests/functional/test_memtable.cpp)
   target_link_libraries(test_memtable lsm_naive)
   add_test(NAME MemTableTest COMMAND test_memtable)
   ```
   Sets up automated testing for components.

### Importance of CMake in the Project

The CMakeLists.txt file serves as the backbone of the build system and directly impacts development workflow:

1. **Cross-Platform Compatibility**

   - Enables building on different operating systems without changing build scripts
   - Manages platform-specific differences automatically

2. **Dependency Management**

   - Ensures components are built in the correct order
   - Makes dependencies explicit and visible

3. **Build Configuration**

   - Allows switching between debug and release builds
   - Enforces consistent compiler settings across the project

4. **Testing Integration**
   - Enables automated test execution
   - Makes it easy to add new tests as development progresses

This file will continue to evolve as more components are added to the project, but the foundation is now in place for a robust build system.
