#!/bin/bash
# Script to run the LSM-Tree CLI interface

# Set default values
implementation="naive"
data_dir="./data/naive"
memtable_size="1048576"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --implementation|-i)
      implementation="$2"
      shift 2
      ;;
    --data-dir|-d)
      data_dir="$2"
      shift 2
      ;;
    --memtable-size|-m)
      memtable_size="$2"
      shift 2
      ;;
    --help|-h)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --implementation, -i <impl>   Implementation to use (naive, compaction, bloom, fence, concurrency)"
      echo "  --data-dir, -d <path>         Directory to store data files"
      echo "  --memtable-size, -m <bytes>   Maximum size of MemTable in bytes"
      echo "  --help, -h                    Show this help"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help to see available options."
      exit 1
      ;;
  esac
done

# Create data directories if they don't exist
mkdir -p data/naive data/compaction data/bloom data/fence data/concurrency

# If a specific data directory was provided, ensure it exists
if [ "$data_dir" != "./data/naive" ]; then
    mkdir -p "$data_dir"
fi

# Set default data directory based on implementation if not specified
if [ "$data_dir" == "./data/naive" ] && [ "$implementation" != "naive" ]; then
    data_dir="./data/$implementation"
    mkdir -p "$data_dir"
fi

echo "Using $implementation implementation"
echo "Data directory: $data_dir"
echo "MemTable size: $memtable_size bytes"

# Run the CLI with the specified options
./build/bin/lsm_cli --implementation "$implementation" --data-dir "$data_dir" --memtable-size "$memtable_size" 