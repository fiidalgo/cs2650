#!/usr/bin/env python3
import os
import sys
import argparse
import shutil

# Add project paths to Python path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')))

try:
    # Try to import the C++ implementation
    from python.lsm_tree.naive import LSMTree
    print("Using C++ implementation of LSM-Tree")
except ImportError:
    print("WARNING: C++ implementation not found. Make sure you have built the C++ module.")
    print("Falling back to pure Python implementation for testing purposes.")
    # This is a fallback for when the C++ implementation is not available
    from lsm_tree_py import LSMTree

from workload_generator import WorkloadGenerator, DistributionType

def main():
    """Run a simple test of the LSM-Tree implementation"""
    
    parser = argparse.ArgumentParser(description='Test the LSM-Tree implementation')
    parser.add_argument('--implementation', type=str, default='naive',
                       help='Implementation to use (naive, compaction, bloom, fence, concurrency)')
    parser.add_argument('--memtable-size', type=int, default=1024 * 1024,
                       help='Maximum size of the MemTable in bytes')
    parser.add_argument('--operation-count', type=int, default=5000,
                       help='Number of operations to perform')
    parser.add_argument('--data-dir', type=str, default='project/data/test',
                       help='Directory to store LSM-Tree data files')
    parser.add_argument('--clean', action='store_true',
                       help='Clean the data directory before running')
    
    args = parser.parse_args()
    
    # Clean the data directory if requested
    if args.clean and os.path.exists(args.data_dir):
        print(f"Cleaning data directory: {args.data_dir}")
        shutil.rmtree(args.data_dir)
    
    # Create the LSM-Tree
    print(f"Using implementation: {args.implementation}")
    print(f"Creating LSM-Tree with MemTable size: {args.memtable_size} bytes")
    lsm_tree = LSMTree(data_dir=args.data_dir, memtable_size_bytes=args.memtable_size)
    
    # Create a workload generator
    print("Creating workload generator")
    generator = WorkloadGenerator(seed=42)
    
    # Generate keys and values
    print(f"Generating {args.operation_count} key-value pairs")
    put_workload, keys = generator.generate_put_workload(
        args.operation_count, 
        distribution=DistributionType.UNIFORM, 
        value_size=100
    )
    
    # Insert all key-value pairs
    print("Inserting key-value pairs...")
    for i, operation in enumerate(put_workload):
        lsm_tree.put(operation["key"], operation["value"])
        
        # Print progress every 1000 operations
        if (i + 1) % 1000 == 0:
            print(f"  Inserted {i + 1}/{len(put_workload)} key-value pairs")
            print(f"  MemTable size: {lsm_tree.get_memtable_size()} bytes")
            print(f"  SSTable count: {lsm_tree.get_sstable_count()}")
    
    # Flush any remaining entries
    print("Flushing remaining entries...")
    lsm_tree.flush()
    
    # Print statistics
    print("\nLSM-Tree statistics:")
    stats = lsm_tree.get_stats()
    for key, value in stats.items():
        print(f"  {key}: {value}")
    
    # Perform some random gets
    print("\nPerforming random gets...")
    num_gets = min(10, len(keys))
    for i in range(num_gets):
        key = keys[i * len(keys) // num_gets]
        value = lsm_tree.get(key)
        print(f"  Get '{key}': {value[:20]}..." if value else f"  Get '{key}': None")
    
    # Perform a range query
    print("\nPerforming a range query...")
    start_idx = len(keys) // 4
    end_idx = start_idx + 10
    start_key = keys[start_idx]
    end_key = keys[end_idx]
    print(f"  Range query from '{start_key}' to '{end_key}':")
    
    count = 0
    for key, value in lsm_tree.range(start_key, end_key):
        print(f"    {key}: {value[:20]}...")
        count += 1
        if count >= 5:  # Limit the output
            print(f"    ... and {end_idx - start_idx - 4} more")
            break
    
    # Close the LSM-Tree
    print("\nClosing LSM-Tree")
    lsm_tree.close()
    
    print("\nTest completed successfully!")

if __name__ == "__main__":
    main() 