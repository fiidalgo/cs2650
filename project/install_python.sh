#!/bin/bash

# Simple script to install the Python bindings for the LSM-Tree

echo "Installing LSM-Tree Python bindings..."

# Make sure the project is built first
if [ ! -d "build" ]; then
    echo "Build directory not found. Building project first..."
    ./build.sh
fi

# Install the Python package in development mode
pip install -e .

echo "Installation complete. You can now import the LSM-Tree in Python:"
echo "  from lsm_tree import naive"
echo "  tree = naive.LSMTree('./data/naive')" 