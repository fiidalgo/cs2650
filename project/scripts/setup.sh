#!/bin/bash

# Environment setup script for the LSM-Tree project
# This script helps set up the project build environment and ensures all dependencies are in place

# Exit immediately if a command exits with a non-zero status
set -e

# Define the project root directory (relative to this script)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Project root: $PROJECT_ROOT"

# Create build directory if it doesn't exist
BUILD_DIR="$PROJECT_ROOT/build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# Check for required tools
echo "Checking required tools..."

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "CMake is required but not installed. Please install CMake first."
    exit 1
else
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    echo "Found CMake version: $CMAKE_VERSION"
fi

# Check for C++ compiler
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "A C++ compiler (g++ or clang++) is required but not installed."
    exit 1
else
    if command -v g++ &> /dev/null; then
        GXX_VERSION=$(g++ --version | head -n1)
        echo "Found g++: $GXX_VERSION"
    fi
    if command -v clang++ &> /dev/null; then
        CLANG_VERSION=$(clang++ --version | head -n1)
        echo "Found clang++: $CLANG_VERSION"
    fi
fi

# Create data directories if they don't exist
for dir in naive compaction bloom fence concurrency; do
    DATA_DIR="$PROJECT_ROOT/data/$dir"
    if [ ! -d "$DATA_DIR" ]; then
        echo "Creating data directory: $DATA_DIR"
        mkdir -p "$DATA_DIR"
    fi
done

# Generate build files using CMake
echo "Generating build files..."
cd "$BUILD_DIR"
cmake ..

echo "Setup complete! You can now build the project using:"
echo "  cd $BUILD_DIR && make"
echo "Run the server with:"
echo "  $BUILD_DIR/bin/server"
echo "And the client with:"
echo "  $BUILD_DIR/bin/client" 