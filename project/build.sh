#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure and build
cmake ..
cmake --build .

# Install the Python module and CLI locally
cmake --install .

echo "Build completed successfully!"
echo "Run the CLI with: ./bin/lsm_cli"
echo "Run the C++ test with: ./build/lsm_tree_test"
echo "Run the Python test with: python tests/python/test_runner.py"
