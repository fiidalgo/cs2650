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

2. **Created the setup.sh environment configuration script**

   - Implemented dependency checking for required tools (CMake, C++ compilers)
   - Automated directory structure creation
   - Added build environment initialization
   - Included helpful usage instructions for developers

3. **Implemented the MemTable header (memtable.h)**

   - Designed the API for the in-memory component of the LSM-Tree
   - Defined operations for putting, retrieving, and removing key-value pairs
   - Implemented range query support for retrieving key ranges efficiently
   - Added support for tombstones using std::optional to mark deleted entries
   - Created a flexible iteration mechanism needed for flushing to disk

4. **Implemented the MemTable source (memtable.cpp)**

   - Created optimized implementations of all interface methods
   - Ensured proper handling of tombstones for deleted entries
   - Used efficient algorithms for range queries with lower_bound
   - Implemented structured code with clear section divisions
   - Used modern C++17 features like structured bindings for cleaner iteration

5. **Designed header files for SSTable, LSM-Tree, and Manifest components**

   - Created interfaces with well-defined responsibilities
   - Established relationships between components
   - Set up the foundation for incremental implementation
   - Added detailed documentation explaining component roles

6. **Implemented a Working Naive LSM-Tree Database**
   - Created a fully interactive database system with the naive implementation
   - Implemented a command-line interface for interacting with the database
   - Added support for key operations: PUT, GET, REMOVE, and RANGE
   - Developed a simple but extensible DSL (Domain-Specific Language) for database commands
   - Implemented support for switching between different implementation types (naive, compaction, etc.)

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

4. **Script Organization**
   - Created a main setup script in `scripts/` directory
   - Reserved subdirectories for phase-specific scripts:
     - `scripts/naive/`: Basic implementation scripts
     - `scripts/compaction/`: Compaction strategy scripts
     - `scripts/bloom/`: Bloom filter testing scripts
     - `scripts/fence/`: Fence pointer testing scripts
     - `scripts/concurrency/`: Concurrency testing scripts

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

### Setup Script Structure and Functionality

The setup.sh script provides a robust environment setup process:

1. **Environment Safety**

   ```bash
   set -e
   ```

   Ensures script exits on any error, preventing partial setups.

2. **Project Root Detection**

   ```bash
   PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
   ```

   Automatically finds the project root directory relative to the script location.

3. **Dependency Verification**

   ```bash
   if ! command -v cmake &> /dev/null; then
       echo "CMake is required but not installed..."
       exit 1
   fi
   ```

   Checks for required tools and provides clear error messages if missing.

4. **Directory Structure**

   ```bash
   for dir in naive compaction bloom fence concurrency; do
       DATA_DIR="$PROJECT_ROOT/data/$dir"
       mkdir -p "$DATA_DIR"
   done
   ```

   Creates necessary directories for each implementation phase.

5. **Build Setup**
   ```bash
   cd "$BUILD_DIR"
   cmake ..
   ```
   Generates the initial build files in the correct location.

### Importance of the Setup Script

The setup.sh script serves several critical purposes in the project:

1. **Environment Consistency**

   - Ensures all developers have the same directory structure
   - Verifies required tools are available
   - Prevents common setup issues

2. **Automation**

   - Reduces manual setup steps
   - Minimizes human error in environment configuration
   - Provides a single command for project initialization

3. **Documentation**

   - Serves as executable documentation of project requirements
   - Makes it clear what tools and structure are needed
   - Provides clear next steps after setup

4. **Maintainability**
   - Centralizes environment configuration
   - Makes it easy to update requirements
   - Simplifies onboarding of new developers

This script, combined with the CMake configuration, provides a solid foundation for the project's build and development environment.

### Component Architecture and Responsibilities

In addition to the MemTable component described earlier, we've designed the following key components for our LSM-Tree implementation:

1. **Header/Implementation Separation**

   - Employed the C++ practice of separating interface (headers) from implementation (source)
   - Headers define the "what" - public API, method signatures, and intended behavior
   - Implementation files contain the "how" - actual algorithms and logic
   - This separation improves maintainability and code organization

2. **SSTable (Sorted String Table)**

   - Purpose: Provides immutable on-disk storage of sorted key-value pairs
   - Design: File-based structure with keys in ascending order for efficient lookups
   - Creation: Generated by flushing MemTables when they reach capacity
   - Operations: Supports read operations including point queries and range scans
   - Key Feature: Immutability allows for optimized sequential I/O and concurrent reads

3. **LSM-Tree (Log-Structured Merge Tree)**

   - Purpose: Serves as the main data structure coordinating in-memory and on-disk components
   - Responsibilities:
     - Manages active MemTable for writes and immutable MemTables awaiting flush
     - Coordinates reads across MemTable and SSTables
     - Triggers compaction when necessary based on configured policy
     - Ensures consistency during all operations
   - Interface: Provides the same operations as MemTable but with persistence guarantees

4. **Manifest**
   - Purpose: Maintains metadata about all SSTables in the system
   - Tracks:
     - SSTable files and their locations
     - Key ranges contained in each SSTable
     - Level assignments for tiered or leveled compaction
     - File sizes and creation timestamps
   - Persistence: Stored in a dedicated file that's updated atomically
   - Recovery: Used to rebuild the system state after shutdown or crash

This component architecture follows the design principles of LSM-Trees while maintaining clean separation of concerns. Each component has a well-defined role in the system, making the implementation easier to understand, test, and extend.

### Client-Server Architecture

The project implements a client-server architecture for the database system:

1. **Server Components**

   - **Main Server (main.cpp)**: Entry point for the server application that initializes the system
   - **Server Logic (server.cpp)**: Handles client connections and request processing
   - **DSL Parser (dsl_parser.cpp)**: Parses the domain-specific language commands from clients
   - Current Status: Placeholder implementation with basic structure defined
   - Future Development: Will handle network communication, command routing, and coordination with the LSM-Tree

2. **Client Component**

   - **Client Application (client.cpp)**: Command-line interface for interacting with the server
   - Current Status: Placeholder implementation with basic structure
   - Future Development: Will provide a user-friendly interface for sending commands to the server

3. **Testing Framework**
   - **Functional Tests**: Validate the behavior of individual components
   - **MemTable Test (test_memtable.cpp)**: Comprehensive test for all MemTable operations:
     - Tests empty table properties
     - Validates put/get operations
     - Tests update behavior
     - Verifies range query functionality
     - Confirms proper tombstone handling for removals
     - Examines iteration with for_each
   - Test Approach: Uses assertions and console output for verification
   - Future Tests: Will cover SSTable, LSM-Tree, and full system integration

### Running the System

The project is designed to be easy to set up and run:

1. **One-Time Setup**

   ```bash
   # Make the setup script executable
   chmod +x scripts/setup.sh

   # Run the setup script to create directories and generate build files
   ./scripts/setup.sh
   ```

   - This verifies prerequisites (CMake, C++ compiler)
   - Creates necessary data directories
   - Generates initial build files
   - Needs to be run only once initially, or after major repository changes

2. **Building the Project**

   ```bash
   # Navigate to the build directory
   cd build

   # Compile the project
   make
   ```

   - Compiles all components (libraries and executables)
   - Must be run after any code changes

3. **Running Tests**

   ```bash
   # Execute the MemTable test
   ./bin/test_memtable
   ```

   - Validates the MemTable implementation
   - Should be run after changes to related components

4. **Running the Server**

   ```bash
   # Start the database server
   ./bin/server
   ```

   - Launches the server process
   - Currently displays a placeholder message
   - Will eventually accept and process client connections

5. **Running the Client**
   ```bash
   # Start the client application
   ./bin/client
   ```
   - Launches the client application
   - Currently displays a placeholder message
   - Will eventually connect to the server and process user commands

All executables are located in the `bin/` directory after building, making it easy to run any component of the system. The system is designed to be built and run on-demand without any persistent state at this stage.

### Implementation Details

1. **Functional Database Components**

   - **LSM-Tree**: Implemented a basic version that uses just the MemTable for storage
   - **Server**: Created a command processor that handles user input and executes database operations
   - **DSL Parser**: Built a parser that converts text commands into structured operations
   - **Command Interface**: Developed a simple but powerful command-line interface

2. **Command Language**
   The database now supports a simple command language:

   ```
   PUT <key> <value>    - Store a key-value pair
   GET <key>            - Retrieve a value by key
   REMOVE <key>         - Delete a key-value pair
   RANGE <start> <end>  - Get all key-value pairs in the range [start, end)
   HELP                 - Show help information
   EXIT                 - Quit the database
   ```

3. **Implementation Selection**
   - Added command-line flag `--impl` to select between implementation types
   - Currently supports: naive, compaction, bloom, fence, concurrency
   - Each implementation has its own data directory for persistence
   - System is designed to make it easy to switch between implementations

### How to Run the System

The database system is now fully interactive and ready to use:

1. **Building the System**

   ```bash
   # From the project root directory
   cmake .
   make
   ```

2. **Running the Database**

   ```bash
   # Start with the default (naive) implementation
   ./bin/server

   # Or specify a different implementation (for future use)
   ./bin/server --impl naive
   ```

3. **Example Usage**

   ```
   > PUT 1 100
   Stored key: 1, value: 100

   > PUT 2 200
   Stored key: 2, value: 200

   > GET 1
   Key: 1, Value: 100

   > RANGE 1 3
   Range query [1, 3) returned 2 entries:
     Key: 1, Value: 100
     Key: 2, Value: 200

   > REMOVE 1
   Removed key: 1

   > GET 1
   Key not found: 1
   ```

4. **Future Enhancements**
   - Add persistence by implementing SSTable flush operations
   - Implement compaction strategies for better performance
   - Add bloom filters to improve read performance
   - Implement fence pointers for more efficient range queries
   - Add multi-threading support for concurrent operations

### Key Achievements

- Created a fully functional, interactive database system
- Successfully implemented all core operations (put, get, remove, range)
- Developed a clean command-line interface
- Established a modular foundation for future optimizations
- Designed a system that can easily switch between implementation types

## April 10th, 2023

### Network Client-Server Implementation

Today I implemented a client-server architecture for our LSM-Tree database, allowing it to operate over a network connection:

#### Server Implementation

- Added socket-based server functionality that listens for client connections on port 9090 (configurable)
- The server can now run in two modes:
  - Console mode (default): Accepts commands directly from the terminal
  - Socket mode: Listens for network connections and processes remote commands
- Modified the server to handle multiple client connections sequentially
- Enhanced the command-line interface to support the new socket mode with options:
  - `--socket` to enable socket server mode
  - `--port` to configure the listening port

#### Client Implementation

- Created a separate client application that connects to the server over a TCP socket
- Implemented a simple command-line interface for the client with shorthand commands:
  - `p <key> <value>` - PUT operation
  - `g <key>` - GET operation
  - `d <key>` - DELETE operation
  - `r <start> <end>` - RANGE operation
  - `s` - Database statistics
  - `h` - Help information
  - `q` - Exit
- The client handles connection establishment, command parsing via the DSL parser, and response display

#### DSL Parser

- Implemented a domain-specific language parser to standardize command interpretation
- The parser handles both shorthand and readable command formats
- Command processing is consistent between console and socket modes

#### Network Protocol

- Used a simple text-based protocol for communication
- Commands are sent as plain text strings
- Responses are returned as formatted text
- This approach provides good human-readability for debugging

#### Usage Instructions

To test the client-server functionality:

1. Start the server in socket mode:
   ```
   ./bin/server --socket
   ```
2. In a separate terminal, start the client:
   ```
   ./bin/client
   ```
3. Enter commands in the client terminal to interact with the database

This implementation provides a foundation for distributed database access while maintaining the same command interface regardless of whether commands come from the console or over the network.
