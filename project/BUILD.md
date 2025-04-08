# Build Instructions

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
