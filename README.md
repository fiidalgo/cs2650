# LSM-Tree Key-Value Database System

## Overview

This project implements a Log-Structured Merge-Tree (LSM-Tree) based key-value database system with a client-server architecture. LSM-Tree systems are optimized for write-heavy workloads hence this system has various optimizations with efficient read operations considered.

## Features

- Leveling merge policy for consistent performance
- Bloom filters for efficient level filtering
- Fence pointers for fast page access within runs
- Support for signed and unsigned key-value pairs
- Client-server architecture with multiple client support
- Data persistence with recovery

## Architecture

### LSM-Tree Structure

- **Memory Buffer**: In-memory storage for recent writes
- **Disk Levels**: Multiple levels with increasing size
- **Leveling**: Each level contains exactly one sorted run
- **Merge Process**: Merges occur when a level reaches capacity

### Optimizations

- **Bloom Filters**: Probabilistic data structure to determine if a key is absent from a level
- **Fence Pointers**: Index structure for efficient binary search within runs
- **Skip List Buffer**: Logarithmic time lookups, maintains sorted order

### Client-Server Model

- **Server**: Manages the LSM-Tree and responds to client queries
- **Client**: Lightweight interface to send queries to the server

## Query Language (DSL)

The system supports the following operations:

- **PUT**: `p [key] [value]` - Insert or update a key-value pair
- **GET**: `g [key]` - Retrieve the value for a given key
- **RANGE**: `r [start_key] [end_key]` - Retrieve all key-value pairs within a range
- **DELETE**: `d [key]` - Remove a key-value pair
- **LOAD**: `l "[file_path]"` - Bulk load key-value pairs from a binary file
- **PRINT STATS**: `s` - Display statistics about the current state of the tree

## Implementation Details

### File Structure

- `server.cpp`: Main server implementation
- `client.cpp`: Client implementation
- `lsm_tree.h/cpp`: Core LSM-Tree implementation
- `bloom_filter.h/cpp`: Bloom filter implementation
- `memory_buffer.h/cpp`: In-memory buffer implementation
- `run.h/cpp`: Sorted runs implementation
- `level.h/cpp`: Level management
- `fence_pointers.h/cpp`: Fence pointer implementation
- `utils.h/cpp`: Utility functions
- `network.h/cpp`: Network communication
- `metadata.h/cpp`: Persistence management

## Building and Running

[Instructions to be added]

## Requirements

[Requirements to be added]
