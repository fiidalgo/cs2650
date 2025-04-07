# LSM-Tree Implementation

This project implements a Log-Structured Merge-Tree (LSM-Tree) storage engine, a fundamental data structure used in many NoSQL databases like LevelDB, RocksDB, Cassandra, and HBase.

## Project Overview

The LSM-Tree provides efficient write and read operations by combining:

- In-memory component (MemTable) for fast writes
- Persistent on-disk components (SSTables) for durable storage
- Background compaction process to maintain performance over time

This implementation features:

- Various implementation strategies (naive, compaction, bloom filters, fence pointers, concurrency)
- C++ core implementations for performance
- Python bindings for easy scripting and testing
- CLI interface for interactive usage

## Getting Started

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10 or higher
- Python 3.6 or higher with development headers
- Git (for cloning the repository)

### Installation

1. Clone the repository:

   ```bash
   git clone <repository-url>
   cd lsm-tree-project
   ```

2. Build the project:

   ```bash
   # Using the build script
   ./build.sh

   # Or manually
   mkdir -p build
   cd build
   cmake ..
   cmake --build .
   cd ..
   ```

3. Install Python bindings (optional):
   ```bash
   ./install_python.sh
   # Or manually
   pip install -e .
   ```

For more detailed build instructions, see [BUILD.md](BUILD.md).

## Usage

### Command-Line Interface

The CLI provides an interactive interface to the LSM-Tree:

```bash
# Run with default settings (naive implementation)
./run_cli.sh

# Run with a specific implementation
./run_cli.sh --implementation naive

# Specify a data directory
./run_cli.sh --data-dir ./data/naive

# For help
./run_cli.sh --help
```

#### Available CLI Commands

- `p <key> <value>` - Put a key-value pair
- `g <key>` - Get value for a key
- `r <start> <end>` - Range query from start to end key (inclusive)
- `d <key>` - Delete a key
- `f` - Flush MemTable to disk
- `c` - Trigger compaction (implementation-dependent)
- `s` - Show statistics
- `l <file>` - Load commands from file
- `q` - Quit the CLI
- `h` - Show help message

### Python API

After installing the Python bindings, you can use the LSM-Tree in Python:

```python
from lsm_tree import naive

# Create an LSM-Tree instance
tree = naive.LSMTree("./data/naive")

# Basic operations
tree.put("key1", "value1")
value = tree.get("key1")
tree.remove("key1")

# Range query
for key, value in tree.range("a", "z"):
    print(f"{key}: {value}")

# Maintenance operations
tree.flush()
tree.compact()
```

### Running Tests

```bash
# Run C++ tests
./build/bin/lsm_tree_test

# Run Python tests
python run_test.py

# Run Python tests with specific implementation
python run_test.py --implementation naive
```

## Project Structure

```
project/
├── include/                # C++ header files
│   ├── naive/              # Core LSM-Tree interface
│   ├── compaction/         # LSM-Tree with compaction
│   ├── bloom/              # LSM-Tree with Bloom filters
│   ├── fence/              # LSM-Tree with fence pointers
│   └── concurrency/        # LSM-Tree with concurrency
│
├── src/                    # C++ implementation files
│   ├── naive/              # Core LSM-Tree implementation
│   ├── compaction/         # Compaction implementation
│   ├── bloom/              # Bloom filters implementation
│   ├── fence/              # Fence pointers implementation
│   ├── concurrency/        # Concurrency implementation
│   ├── bindings/           # Python bindings (pybind11)
│   └── cli.cpp             # Command-line interface
│
├── python/                 # Python package
│   └── lsm_tree/           # LSM-Tree Python module
│
├── tests/                  # Test files
│   ├── cpp/                # C++ tests
│   └── python/             # Python tests
│
├── data/                   # Data storage directories
│   ├── naive/              # Data for naive implementation
│   ├── compaction/         # Data for compaction implementation
│   ├── bloom/              # Data for bloom filter implementation
│   ├── fence/              # Data for fence pointers implementation
│   └── concurrency/        # Data for concurrent implementation
│
├── docs/                   # Documentation
│   ├── work_log.md         # Development log
│   ├── project-plan.md     # Project plan
│   └── design-docs/        # Design documentation
│
├── build/                  # Build artifacts (generated)
│
├── CMakeLists.txt          # CMake build configuration
├── setup.py                # Python package setup
├── build.sh                # Build script
├── install_python.sh       # Python installation script
├── run_cli.sh              # CLI runner script
├── run_test.py             # Test runner script
├── BUILD.md                # Build instructions
└── README.md               # This file
```

## Implementation Details

### Key Components

1. **MemTable**

   - In-memory sorted data structure using `std::map`
   - Supports put, get, range, and size operations
   - Tracks total memory usage for flush decisions

2. **SSTable (Sorted String Table)**

   - On-disk immutable storage format
   - Contains header with metadata (JSON format)
   - Stores key-value pairs in sorted order
   - Supports efficient lookup and range queries

3. **LSM-Tree**
   - Manages MemTable and multiple SSTables
   - Implements the core API (put, get, remove, range)
   - Handles flushing MemTable to disk when needed
   - Implements (or will implement) various optimizations

### Implementation Variations

- **Naive**: Basic implementation with MemTable and SSTables, no compaction
- **Compaction**: Adds leveled compaction to improve read performance
- **Bloom Filters**: Uses Bloom filters to reduce unnecessary disk reads
- **Fence Pointers**: Adds sparse indexing for faster range scans
- **Concurrency**: Supports concurrent operations with proper synchronization

## Contributing

To add a new implementation:

1. Create header files in `include/<implementation>/`
2. Add implementation files in `src/<implementation>/`
3. Create a Python wrapper in `python/lsm_tree/`
4. Update the factory in `cli.cpp` to support the new implementation
5. Add tests for the new implementation

## License

This project is for educational purposes as part of CS2650.
