# LSM-Tree Implementation Work Log

This document tracks the development progress of our LSM-Tree implementation project. It includes details on project structure, implementation decisions, and future plans.

## Project Structure

The project is organized as follows:

```
project/
├── include/                # C++ header files
│   ├── naive/
│   │   ├── memtable.h      # In-memory storage component
│   │   ├── sstable.h       # On-disk storage component
│   │   └── lsm_tree.h      # Main LSM-Tree implementation
│   ├── compaction/         # (Future) Headers for compaction-enabled implementation
│   ├── bloom/              # (Future) Headers for Bloom filters implementation
│   ├── fence/              # (Future) Headers for fence pointers implementation
│   └── concurrency/        # (Future) Headers for concurrent implementation
│
├── src/                    # C++ implementation files
│   ├── naive/
│   │   ├── memtable.cpp    # MemTable implementation
│   │   ├── sstable.cpp     # SSTable implementation
│   │   └── lsm_tree.cpp    # LSM-Tree implementation
│   ├── cli.cpp             # Command-line interface
│   └── bindings/           # Python bindings using pybind11
│
├── data/                   # Data storage directories
│   ├── naive/              # Storage for naive implementation
│   ├── compaction/         # Storage for compaction implementation
│   ├── bloom/              # Storage for bloom filter implementation
│   ├── fence/              # Storage for fence pointers implementation
│   └── concurrency/        # Storage for concurrent implementation
│
├── tests/                  # Test files
│   ├── cpp/                # C++ tests
│   └── python/             # Python tests
│
├── python/                 # Python package
│   └── lsm_tree/           # LSM-Tree Python module
│
├── docs/                   # Documentation
│   ├── work_log.md         # This file
│   ├── project-plan.md     # Project plan
│   └── design-docs/        # Design documentation
```

## Completed Components

### 1. Naive Implementation

The naive implementation includes three main components:

#### 1.1 MemTable (in-memory component)

**Files**: `include/naive/memtable.h`, `src/naive/memtable.cpp`

**Functionality**:

- Uses `std::map` for storing key-value pairs in sorted order
- Supports operations: `put`, `get`, `range`, `size`, `sizeBytes`, `clear`
- Tracks total memory usage for flush decisions
- Provides iterators for accessing data sequentially

**Key Methods**:

- `bool put(const std::string& key, const std::string& value)`: Insert or update a key-value pair
- `std::optional<std::string> get(const std::string& key) const`: Retrieve a value for a key
- `void range(const std::string& start_key, const std::string& end_key, callback)`: Range query
- `size_t size() const`: Get number of entries
- `size_t sizeBytes() const`: Get total size in bytes
- `void clear()`: Remove all entries

#### 1.2 SSTable (on-disk component)

**Files**: `include/naive/sstable.h`, `src/naive/sstable.cpp`

**Functionality**:

- Immutable on-disk storage of sorted key-value pairs
- File format includes metadata header and key-value pairs
- Supports efficient lookups and range queries
- Handles serialization and deserialization

**Key Methods**:

- `static std::unique_ptr<SSTable> createFromMemTable(...)`: Create SSTable from MemTable
- `static std::unique_ptr<SSTable> load(const std::string& file_path)`: Load SSTable from disk
- `std::optional<std::string> get(...)`: Look up a key
- `void range(...)`: Range query
- `size_t getSizeBytes() const`: Get file size
- `void remove() const`: Delete SSTable file

#### 1.3 LSM-Tree (main component)

**Files**: `include/naive/lsm_tree.h`, `src/naive/lsm_tree.cpp`

**Functionality**:

- Manages one MemTable and multiple SSTables
- Implements the main key-value store operations
- Handles MemTable flushing when size threshold is exceeded
- Implements tombstone-based deletion

**Key Methods**:

- `void put(const std::string& key, const std::string& value)`: Insert or update
- `std::optional<std::string> get(const std::string& key, ...)`: Retrieve a value
- `void range(...)`: Range query
- `void remove(const std::string& key)`: Delete a key (using tombstone)
- `void flush()`: Manually flush MemTable to disk
- `void compact()`: Trigger compaction (empty in naive implementation)
- `std::string getStats() const`: Get statistics as JSON

### 2. Command Line Interface

**File**: `src/cli.cpp`

**Functionality**:

- Interactive command-line interface to LSM-Tree
- Supports multiple implementations via factory pattern
- Provides commands for all LSM-Tree operations

**Commands**:

- `p <key> <value>`: Put a key-value pair
- `g <key>`: Get value for a key
- `r <start> <end>`: Range query from start to end key (inclusive)
- `d <key>`: Delete a key
- `f`: Flush MemTable to disk
- `c`: Trigger compaction (implementation-dependent)
- `s`: Show statistics
- `l <file>`: Load commands from file
- `q`: Quit
- `h`: Show help

**Edge Cases**:

- Compaction (`c` command) in the naive implementation does nothing, with a message indicating compaction isn't implemented
- Non-existent keys in `g` command return "Key not found"
- Invalid commands show usage help
- Range queries with no results display "0 results found"

## Future Implementations

### 1. Compaction Implementation

Planned features:

- Leveled compaction strategy
- Merging of SSTables to improve read performance
- Background compaction thread

### 2. Bloom Filters Implementation

Planned features:

- Bloom filter for each SSTable
- Probabilistic filtering to avoid unnecessary disk reads
- Configurable false positive rate

### 3. Fence Pointers Implementation

Planned features:

- Sparse indexing of SSTable data
- Efficient range queries with reduced disk I/O
- Binary search for key lookups within SSTables

### 4. Concurrency Implementation

Planned features:

- Thread-safe operations
- Background compaction
- Lock-free reads where possible

## Development Log

### 2025-04-06: Project Setup and Naive Implementation

1. Set up project structure
2. Implemented the basic naive LSM-Tree with MemTable and SSTable
3. Created command-line interface
4. Added Python bindings
5. Organized data directories for different implementations
6. Updated documentation

### Next Steps

1. Implement leveled compaction
2. Add Bloom filters for optimized reads
3. Implement fence pointers for efficient range queries
4. Add concurrency support
5. Conduct performance evaluations

### 2025-04-07: Compaction Implementation