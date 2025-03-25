# LSM-Tree Research Notes

## Table of Contents

1. [LSM-Tree Fundamentals](#lsm-tree-fundamentals)
2. [Performance Characteristics](#performance-characteristics)
3. [Project Requirements](#project-requirements)

## LSM-Tree Fundamentals

### Basic Structure and Concepts

LSM-Trees (Log-Structured Merge Trees) are data structures designed to provide high write throughput while maintaining reasonable read performance. They achieve this by organizing data in multiple levels with increasing sizes.

#### Core Components:

1. **MemTable (L0)**:

   - In-memory buffer that stores recent writes
   - Typically implemented as a balanced tree (e.g., skiplist, red-black tree, B+ tree)
   - Provides fast write operations by avoiding disk I/O
   - Once full, it's flushed to disk as an immutable SSTable (Sorted String Table)

2. **SSTables (Sorted String Tables)**:

   - Immutable files on disk containing sorted key-value pairs
   - Organized into levels (L1, L2, ..., Ln)
   - Each level has a size limit, typically increasing by a factor (size ratio) at each level
   - Keys are sorted within each SSTable, enabling binary search

3. **Levels**:
   - L1: Contains SSTables flushed from MemTable
   - L2-Ln: Contain SSTables merged from previous levels
   - Size increases exponentially with level (e.g., L2 is 10x larger than L1)

### Existing Implementations

1. **LevelDB (Google)**:

   - Basic implementation of LSM-Tree
   - Uses leveling merge policy
   - Includes Bloom filters and fence pointers
   - Single-threaded compaction

2. **RocksDB (Facebook)**:

   - Fork of LevelDB with many enhancements
   - Multi-threaded compaction
   - Improved caching
   - Pluggable merge policies
   - Column families

3. **Cassandra (Apache)**:
   - Distributed NoSQL database using LSM-Tree
   - Uses tiering merge policy (SizeTieredCompactionStrategy)
   - Also offers leveling (LeveledCompactionStrategy)
   - Distributed architecture with tunable consistency

### Core Operations

1. **Write (Put)**:

   - Insert key-value pair into MemTable
   - If MemTable is full, flush to disk as new SSTable in L1
   - May trigger compaction if level size thresholds are exceeded

2. **Read (Get)**:

   - Search MemTable first
   - If not found, search SSTables in order from L1 to Ln
   - Use Bloom filters to skip SSTables that don't contain the key
   - Use fence pointers to narrow search within SSTables

3. **Range Query**:

   - Similar to read but returns multiple key-value pairs in a range
   - Requires merging results from multiple SSTables

4. **Delete**:

   - Insert a "tombstone" marker in MemTable
   - During compaction, tombstones remove matching keys from lower levels

5. **Merge/Compaction**:
   - Process of combining SSTables to maintain structure
   - Removes duplicates and applies tombstones
   - Critical for maintaining read performance

### Bloom Filters and Fence Pointers

1. **Bloom Filters**:

   - Probabilistic data structure that tests if an element is in a set
   - Can have false positives but no false negatives
   - Used to quickly determine if a key might exist in an SSTable
   - Reduces unnecessary disk I/O by skipping SSTables that don't contain a key

2. **Fence Pointers**:
   - Index structure that divides SSTables into blocks
   - Stores the first key of each block
   - Allows binary search to quickly locate the block containing a key
   - Reduces the amount of data read from disk

### Merge Policies

1. **Leveling**:

   - Each level contains at most one sorted run
   - When level L exceeds its size limit, merge some files with level L+1
   - Lower write amplification but higher read amplification
   - Example: LevelDB, RocksDB (default)

2. **Tiering**:

   - Each level can contain multiple sorted runs
   - When level L has too many runs, merge all runs and move to level L+1
   - Higher write amplification but lower read amplification
   - Example: Cassandra's SizeTieredCompactionStrategy

3. **Lazy Leveling**:
   - Hybrid approach combining aspects of leveling and tiering
   - Allow multiple runs per level but eventually merge to one run
   - Balances read and write amplification
   - Example: Dostoevsky algorithm

## Performance Characteristics

### Read vs. Write Tradeoffs

1. **Write Optimization**:

   - LSM-Trees excel at write-heavy workloads
   - Sequential writes are faster than random writes on disk
   - In-memory buffer absorbs write bursts
   - Tiering favors write performance over read performance

2. **Read Challenges**:

   - May need to check multiple levels (read amplification)
   - Bloom filters help reduce unnecessary I/O
   - Leveling favors read performance over write performance
   - Caching frequently accessed data improves read performance

3. **Quantifying Tradeoffs**:
   - **Read Amplification**: Number of disk reads per logical read
   - **Write Amplification**: Number of disk writes per logical write
   - **Space Amplification**: Ratio of actual storage used to logical data size

### Size Ratio Optimization

1. **Definition**:

   - Ratio of size between adjacent levels (e.g., L2 is T times larger than L1)
   - Typically ranges from 2 to 10

2. **Impact**:

   - Higher ratio: Fewer levels, higher write amplification, lower read amplification
   - Lower ratio: More levels, lower write amplification, higher read amplification

3. **Optimal Selection**:
   - Depends on workload characteristics (read/write ratio)
   - Monkey paper suggests optimal bits-per-key for Bloom filters based on size ratio

### Space Amplification Issues

1. **Causes**:

   - Duplicate keys across levels
   - Tombstone markers for deleted keys
   - Partially filled SSTables

2. **Mitigation Strategies**:
   - Frequent compaction reduces duplicates but increases write amplification
   - Efficient encoding of keys and values
   - Compression of SSTables
   - Strategic tombstone cleanup

### Merge/Compaction Strategies

1. **Full Compaction**:

   - Merge all files in level L with all files in level L+1
   - High I/O cost but optimal space usage

2. **Partial Compaction**:

   - Select subset of files from level L to merge with overlapping files in level L+1
   - Lower I/O cost but potentially higher space amplification
   - Examples: RocksDB's universal compaction, LevelDB's size-tiered approach

3. **Compaction Scheduling**:

   - Background process to minimize impact on foreground operations
   - Can be triggered by:
     - Level size thresholds
     - Number of files in a level
     - Age of data
     - System idle time

4. **Compaction Parallelism**:
   - Multiple compaction threads for different key ranges
   - Careful synchronization required
   - Can utilize multiple CPU cores and I/O channels

## Project Requirements

### DSL Specifications

The key-value store must support a Domain Specific Language (DSL) with the following commands:

1. **Put**: `p [key] [value]` - Insert or update a key-value pair
2. **Get**: `g [key]` - Retrieve the value for a given key
3. **Range**: `r [start] [end]` - Retrieve all key-value pairs in the given range
4. **Delete**: `d [key]` - Delete a key-value pair
5. **Load**: `l [filepath]` - Bulk load data from a file
6. **Stats**: `s` - Print statistics about the database

### Workload Generator and Test Framework

1. **Key Characteristics**:

   - Support for both signed and unsigned keys and values
   - Various distributions (uniform, skewed, sequential)
   - Configurable read/write ratios

2. **Test Framework Requirements**:
   - Correctness validation
   - Performance measurement
   - Scalability testing with multiple clients

### Required API Endpoints

1. **Put Operation**:

   - Insert or update a key-value pair
   - Handle duplicates by overwriting
   - Support for various data types

2. **Get Operation**:

   - Retrieve value for a specific key
   - Return error or null if key doesn't exist
   - Efficient lookup using Bloom filters and fence pointers

3. **Range Operation**:

   - Return all key-value pairs within a range
   - Efficiently merge results from multiple levels
   - Support for pagination or limiting results

4. **Delete Operation**:

   - Remove a key-value pair
   - Implement using tombstone markers
   - Eventually purge during compaction

5. **Load Operation**:

   - Bulk import data from a file
   - More efficient than individual puts
   - Format specification for input file

6. **Stats Operation**:
   - Report system statistics
   - Include metrics like:
     - Total key count
     - Size on disk
     - Memory usage
     - Level information
     - Performance counters

### System Architecture Requirements

1. **Client-Server Model**:

   - Database server manages data and processes queries
   - Lightweight query clients communicate with server
   - Support for multiple concurrent clients

2. **Data Persistence**:

   - Store data in specified directory
   - Recover state on restart
   - Rebuild auxiliary structures (Bloom filters, fence pointers)

3. **Performance Goals**:
   - Optimize for write throughput
   - Maintain reasonable read latency
   - Scale with multiple cores (in part 2)
   - Handle datasets from 100MB to 10GB
