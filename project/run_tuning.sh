#!/bin/bash
set -e  # Exit on error

# Clean up any previous builds
echo "Cleaning previous build files..."
rm -rf build
mkdir -p build
cd build

# Build the project
echo "Building project..."
cmake .. || {
    echo "CMake configuration failed. Trying to fix missing test files..."
    # Try again with the fixed CMakeLists.txt
    cd ..
    cd build
    cmake ..
}

# Build the tuning tests
echo "Building tuning tests..."
make -j compaction_tuning_tests || {
    echo "Failed to build tuning tests. Exiting."
    exit 1
}

# Create data directory for compaction tests
mkdir -p ../../data/compaction

# Try to set the library path for GTest
GTEST_DIR=$(find /Users/nico/miniconda3 -name "libgtest_main*.dylib" -o -name "libgtest_main*.so" | head -n 1 | xargs dirname 2>/dev/null)
if [ -n "$GTEST_DIR" ]; then
    echo "Found GTest library in: $GTEST_DIR"
    export DYLD_LIBRARY_PATH="$GTEST_DIR:$DYLD_LIBRARY_PATH"
else
    echo "GTest library not found. Tests may fail."
fi

# Run the tuning tests one by one, continuing even if some fail
echo "Running L0 threshold tuning test..."
./bin/compaction_tuning_tests --gtest_filter=CompactionTuningTest.L0ThresholdTuning || echo "L0 threshold test failed."

echo "Running size ratio tuning test..."
./bin/compaction_tuning_tests --gtest_filter=CompactionTuningTest.SizeRatioTuning || echo "Size ratio test failed."

echo "Running policy tuning test..."
./bin/compaction_tuning_tests --gtest_filter=CompactionTuningTest.PolicyTuning || echo "Policy tuning test failed."

echo "Running range query tuning test..."
./bin/compaction_tuning_tests --gtest_filter=CompactionTuningTest.RangeQueryTuning || echo "Range query test failed."

# Go back to project directory
cd ..

# Check for Python dependencies before running visualization
echo "Checking Python dependencies..."
pip list | grep -q seaborn || {
    echo "Installing missing seaborn package..."
    pip install seaborn
}

# Run the visualization script
echo "Generating visualization plots..."
python3 tests/python/visualize_tuning.py

echo "Tuning completed. Check the ../data/compaction/tuning_* directory for results." 