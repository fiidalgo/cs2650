# Build Instructions

This document provides detailed instructions for building and running the LSM-Tree project.

## System Requirements

- **Operating Systems**: Linux, macOS, or Windows
- **Compiler**: C++17 compatible compiler
  - GCC 7.0 or higher
  - Clang 5.0 or higher
  - MSVC 2017 or higher
- **Build System**: CMake 3.10 or higher
- **Python**: Python 3.6 or higher with development headers (for Python bindings)
- **Dependencies** (automatically downloaded via CMake):
  - pybind11 (for Python bindings)
  - nlohmann_json (for JSON handling)

## Building the Project

### Option 1: Using the Build Script

The simplest way to build the project is using the provided build script:

```bash
./build.sh
```

This script will create a build directory, run CMake, and build the project.

### Option 2: Manual Build

For more control over the build process:

1. Create a build directory:

   ```bash
   mkdir -p build
   cd build
   ```

2. Configure the project:

   ```bash
   cmake ..
   ```

   Optional CMake flags:

   - `-DCMAKE_BUILD_TYPE=Release` - Build in release mode (optimized)
   - `-DCMAKE_BUILD_TYPE=Debug` - Build in debug mode (with debug symbols)
   - `-DBUILD_TESTS=OFF` - Disable building tests
   - `-DBUILD_PYTHON_BINDINGS=OFF` - Disable Python bindings

3. Build the project:

   ```bash
   cmake --build .
   ```

4. (Optional) Install locally:

   ```bash
   cmake --install . --prefix=/path/to/install
   ```

5. Return to the project root:
   ```bash
   cd ..
   ```

## Python Bindings

### Installing Python Bindings

To use the LSM-Tree from Python:

```bash
# Using the script
./install_python.sh

# Or manually
pip install -e .
```

This will install the package in development mode, allowing you to modify the code without reinstalling.

## Running the Project

### Command-Line Interface (CLI)

The CLI allows you to interact with the LSM-Tree implementations:

```bash
# Run with default settings
./run_cli.sh

# Run with specific implementation and data directory
./run_cli.sh --implementation naive --data-dir ./data/naive

# Show available options
./run_cli.sh --help
```

CLI options:

- `--implementation, -i <impl>` - Implementation to use (naive, compaction, bloom, fence, concurrency)
- `--data-dir, -d <path>` - Directory to store data files
- `--memtable-size, -m <bytes>` - Maximum size of MemTable in bytes
- `--help, -h` - Show help message

### Running Tests

```bash
# C++ tests
./build/bin/lsm_tree_test

# Python tests (all implementations)
python run_test.py

# Python tests (specific implementation)
python run_test.py --implementation naive
```

## Troubleshooting

### Common Build Issues

1. **CMake not found**: Ensure CMake is installed and in your PATH.

   ```bash
   cmake --version
   ```

2. **Compiler not compatible**: Check your compiler version.

   ```bash
   # GCC
   g++ --version
   # Clang
   clang++ --version
   ```

3. **Python headers missing**: Install Python development packages.

   ```bash
   # Ubuntu/Debian
   sudo apt-get install python3-dev
   # macOS (Homebrew)
   brew install python
   ```

4. **Build errors related to dependencies**: Clear the build directory and try again.
   ```bash
   rm -rf build
   mkdir build
   ```

### Getting Help

If you encounter issues not covered here, please check the project documentation or open an issue in the repository.
