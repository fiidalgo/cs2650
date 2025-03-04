# LSM-Tree Key-Value Store Design Document

## 1. Design Description

### 1.1 Introduction

This project involves the development of a modern write-optimized NoSQL key-value store based on the Log-Structured Merge Tree (LSM-Tree) architecture. The system is designed to provide high write throughput while maintaining reasonable read performance, following the specifications outlined in the Monkey paper. The implementation includes a single-threaded key-value store with all major components of state-of-the-art key-value stores, including optimized Bloom filters, fence pointers, and multiple merge policies. The system operates as a client-server architecture, with a database server that manages data and processes queries, and lightweight clients that communicate using a specified Domain Specific Language (DSL). The design prioritizes write optimization, data persistence, and efficient query processing for both point and range queries.

### 1.2 Technical Description

#### 1.2.1 In-Memory Buffer (MemTable) Design

**Design Considerations:**

- The MemTable serves as the primary write buffer, absorbing all incoming writes to avoid immediate disk I/O
- It must support efficient insertion, lookup, and range scan operations
- It needs to be serializable for persistence and recovery

**Design Elements:**

- **Data Structure Selection:** We chose a Skip List for the MemTable implementation due to its balanced performance characteristics (O(log n) operations), memory efficiency, and implementation simplicity compared to alternatives like red-black trees or B+ trees. Skip Lists also provide natural support for range queries and are easier to extend for concurrent access in future iterations.

- **Memory Layout:**

  - Each Skip List node contains a key, value, tombstone flag, and pointers to next nodes
  - Keys and values are stored as variable-length byte arrays to support different data types
  - A size tracker monitors both entry count and memory consumption
  - A Write-Ahead Log (WAL) ensures durability by recording operations before they're applied to the MemTable

- **Flushing Mechanism:**
  - The MemTable is flushed to disk when it reaches configurable size thresholds (default: 4MB or 1,000,000 entries)
  - During flush, the current MemTable becomes immutable, a new MemTable is created for incoming writes, and the immutable MemTable is serialized to disk as an SSTable
  - This approach ensures continuous write availability while flushing occurs

**Implementation Approach:**

- The Skip List will be implemented with a probabilistic level assignment mechanism
- The WAL will use a simple append-only file format with checksums for integrity
- Flushing will be triggered asynchronously to avoid blocking the write path

#### 1.2.2 On-Disk Data Layout

**Design Considerations:**

- The on-disk format must support efficient reads, especially for range queries
- It needs to include auxiliary structures (Bloom filters, fence pointers) for performance
- The format should be resilient to crashes and support recovery

**Design Elements:**

- **SSTable Structure:**

  - **Header:** Contains metadata including magic number, format version, timestamp, entry count, min/max keys, section offsets, and checksums
  - **Data Section:** Stores sorted key-value pairs with each entry containing key length, key, value length, value, and tombstone flag
  - **Index Section:** Contains fence pointers (first key of each block and its offset) to enable binary search within the file
  - **Bloom Filter Section:** Stores a serialized Bloom filter for the entire SSTable with level-specific bit-per-entry ratios
  - **Footer:** Includes file checksum and size information

- **Level Organization:**

  - Multiple levels with increasing size (configurable size ratio T, default T=10)
  - Hybrid merge policy approach:
    - L1: Tiering (multiple sorted runs) for write optimization
    - L2: Lazy Leveling (hybrid approach) for balanced performance
    - L3+: Leveling (single sorted run per level) for read optimization

- **File Management:**
  - A manifest file tracks all SSTables across levels, recording creation, deletion, and compaction operations
  - An in-memory catalog maps levels to SSTables and key ranges
  - Files follow a naming convention (e.g., `L1-1620000000-001.sst`) for easy identification

**Implementation Approach:**

- SSTables will be implemented as immutable files with a well-defined binary format
- The manifest will use a journal-based approach for crash resilience
- The catalog will be reconstructed from the manifest during startup

#### 1.2.3 Query Processing

**Design Considerations:**

- Queries must be processed efficiently across multiple levels
- The system needs to handle both point queries (get) and range queries
- Deleted keys must be properly handled throughout the system

**Design Elements:**

- **Read Path:**

  - Searches proceed from newest to oldest data: MemTable → immutable MemTables → SSTables (L1 to Ln)
  - Bloom filters are used to skip SSTables that don't contain the target key
  - Fence pointers narrow the search to specific blocks within SSTables
  - For range queries, results from multiple sources are merged with duplicate elimination

- **Write Path:**
  - All writes (put, update, delete) go to the MemTable first
  - Deletes are implemented as tombstone markers
  - Writes are logged to the WAL before being applied to the MemTable
  - Flush operations are triggered when size thresholds are exceeded

**Implementation Approach:**

- The query processor will be implemented as a separate component that coordinates across all levels
- A merging iterator pattern will be used for range queries to efficiently combine results
- Tombstone handling will be integrated into both read and compaction paths

#### 1.2.4 Compaction Strategy

**Design Considerations:**

- Compaction is critical for maintaining read performance over time
- It must balance I/O costs with space reclamation
- Different levels may benefit from different compaction approaches

**Design Elements:**

- **Compaction Triggers:**

  - Size-based: Level exceeds size threshold
  - Count-based: Number of files exceeds threshold
  - Time-based: Periodic compaction for space reclamation

- **Compaction Process:**
  - Files are selected based on the level's merge policy
  - A k-way merge algorithm combines data from selected files
  - Duplicates are eliminated (keeping newest versions)
  - Tombstones remove matching entries from lower levels
  - Partial compaction is used for deeper levels to reduce I/O costs

**Implementation Approach:**

- Compaction will run as a background process to minimize impact on foreground operations
- A priority queue-based merger will efficiently combine data from multiple files
- File selection will use heuristics to minimize compaction costs while maximizing benefits

### 1.3 Additional Optimizations

#### 1.3.1 Optimization 1: Adaptive Merge Policy Selection

**Design Considerations:**

- Workloads can vary significantly in their read/write ratios
- Fixed merge policies may not be optimal for all workloads
- Dynamic adaptation can improve overall performance

**Design Elements:**

- **Workload Monitoring:**

  - Track read/write ratio over configurable time windows
  - Maintain historical patterns to detect trends

- **Policy Adjustment:**
  - For write-heavy workloads, favor tiering in more levels
  - For read-heavy workloads, favor leveling in more levels
  - Implement smooth transitions between policies to avoid performance cliffs

**Implementation Approach:**

- A workload analyzer component will collect operation statistics
- Policy decisions will be made periodically based on recent workload patterns
- Configuration parameters will allow tuning the sensitivity of adaptation

#### 1.3.2 Optimization 2: Block Cache with Frequency-Based Eviction

**Design Considerations:**

- Repeated reads of the same data are common in many workloads
- Disk I/O is a major performance bottleneck for reads
- Different components (data blocks, index blocks, Bloom filters) have different access patterns

**Design Elements:**

- **Cache Structure:**

  - Separate caches for data blocks, index blocks, and Bloom filters
  - LFU (Least Frequently Used) eviction policy for data blocks
  - Configurable cache size (default: 10% of heap)

- **Cache Management:**
  - Frequency counters with decay to favor recently accessed items
  - Prefetching for sequential access patterns
  - Pinning for critical metadata

**Implementation Approach:**

- The cache will be implemented using a hash table with frequency counters
- A background thread will periodically adjust frequency counts to favor recent accesses
- Cache statistics will be exposed through the stats command

#### 1.3.3 Optimization 3: Selective Compression

**Design Considerations:**

- Compression can significantly reduce storage requirements and I/O costs
- Different data types have different compression characteristics
- Compression CPU overhead can be significant

**Design Elements:**

- **Data Sampling:**

  - Sample data blocks to determine compressibility
  - Classify data based on entropy and patterns

- **Algorithm Selection:**

  - LZ4 for speed-sensitive operations
  - Zstandard for better compression ratio when CPU is available
  - No compression for already-compressed or random data

- **Selective Application:**
  - Apply compression at block level rather than file level
  - Skip compression for blocks with low compressibility
  - Use different algorithms for different levels (more compression for deeper levels)

**Implementation Approach:**

- A compression manager will handle algorithm selection and application
- Compression decisions will be recorded in block metadata
- Compression statistics will be tracked and exposed through the stats command

### 1.4 Challenges and Solutions

#### 1.4.1 Balancing Memory Usage

**Challenge:** Managing memory usage across MemTable, block cache, Bloom filters, and other in-memory structures is complex and can lead to out-of-memory conditions or suboptimal performance.

**Solution:** We will implement a memory budget system that allocates memory across components based on their importance and usage patterns. The system will dynamically adjust allocations based on workload characteristics and available memory. Critical components like the MemTable will have guaranteed minimum allocations, while others like caches can scale up or down as needed.

#### 1.4.2 Ensuring Durability Without Sacrificing Performance

**Challenge:** Ensuring data durability typically requires synchronous disk writes, which can significantly impact write performance.

**Solution:** We will use a combination of techniques:

- Group commit for WAL entries to amortize fsync costs across multiple operations
- Configurable durability levels (from fully synchronous to fully asynchronous)
- Checksums and careful ordering of writes to ensure crash recovery is possible
- Background flushing with careful coordination to maintain consistency

#### 1.4.3 Handling Compaction Efficiently

**Challenge:** Compaction can consume significant I/O and CPU resources, potentially impacting foreground operations.

**Solution:** Our approach includes:

- Rate limiting for compaction I/O to avoid starving foreground operations
- Intelligent scheduling based on system load and idle periods
- Partial compaction strategies to reduce resource requirements
- Prioritization based on expected benefit (e.g., focusing on levels with more overlap)

#### 1.4.4 Supporting Both Point and Range Queries Efficiently

**Challenge:** Optimizing for both point queries and range queries often involves different and sometimes conflicting design decisions.

**Solution:** We will address this through:

- Hybrid merge policies that balance the needs of both query types
- Bloom filters for efficient point query filtering
- Fence pointers and block-based organization for efficient range queries
- Caching strategies that recognize and adapt to different query patterns

## 2. Design Decisions Documentation

### 2.1 Overall Architecture Diagram

```
┌─────────────────┐     ┌─────────────────┐
│     Client      │     │     Client      │
└────────┬────────┘     └────────┬────────┘
         │                       │
         │                       │
         │                       │
┌────────▼───────────────────────▼────────┐
│                Server                    │
│  ┌─────────────────────────────────┐    │
│  │          Query Processor        │    │
│  └─────────────┬───────────────────┘    │
│                │                         │
│  ┌─────────────▼───────────────────┐    │
│  │         LSM-Tree Engine         │    │
│  │  ┌─────────┐    ┌────────────┐  │    │
│  │  │MemTable │    │  Immutable │  │    │
│  │  │(SkipList)│    │  MemTable  │  │    │
│  │  └────┬────┘    └─────┬──────┘  │    │
│  │       │               │         │    │
│  │  ┌────▼───────────────▼──────┐  │    │
│  │  │       Disk Manager        │  │    │
│  │  └────────────┬──────────────┘  │    │
│  │               │                  │    │
│  │  ┌────────────▼──────────────┐  │    │
│  │  │      Compaction Manager   │  │    │
│  │  └────────────┬──────────────┘  │    │
│  └───────────────┼─────────────────┘    │
│                  │                       │
│  ┌───────────────▼─────────────────┐    │
│  │        Storage Engine           │    │
│  └───────────────┬─────────────────┘    │
└──────────────────┼────────────────────┬─┘
                   │                    │
       ┌───────────▼────────────┐    ┌─▼─────────┐
       │     Data Directory     │    │    WAL    │
       │  ┌─────────────────┐  │    └───────────┘
       │  │      Level 1    │  │
       │  │  (Tiering)      │  │
       │  └─────────────────┘  │
       │  ┌─────────────────┐  │
       │  │      Level 2    │  │
       │  │  (Lazy Leveling) │  │
       │  └─────────────────┘  │
       │  ┌─────────────────┐  │
       │  │    Level 3+     │  │
       │  │    (Leveling)   │  │
       │  └─────────────────┘  │
       │  ┌─────────────────┐  │
       │  │     Manifest    │  │
       │  └─────────────────┘  │
       └────────────────────────┘
```

### 2.2 Data Structures and Algorithms

| Component      | Data Structure | Time Complexity                     | Space Complexity          |
| -------------- | -------------- | ----------------------------------- | ------------------------- |
| MemTable       | Skip List      | O(log n) search/insert/delete       | O(n)                      |
| Bloom Filter   | Bit Array      | O(k) lookup, k = hash functions     | O(m), m = bits            |
| Fence Pointers | Array          | O(log b) binary search, b = blocks  | O(b)                      |
| Compaction     | K-way Merge    | O(n log k), n = entries, k = files  | O(B + k), B = buffer size |
| Range Query    | Heap Merge     | O(n log L), n = entries, L = levels | O(L)                      |

### 2.3 Complexity Analysis for Operations

| Operation  | Average Case | Worst Case            | Notes                             |
| ---------- | ------------ | --------------------- | --------------------------------- |
| Put        | O(log m)     | O(log m + f)          | m = MemTable size, f = flush cost |
| Get        | O(log m + L) | O(log m + L \* log f) | L = levels, f = files per level   |
| Range      | O(k log L)   | O(k log (L \* f))     | k = result size                   |
| Delete     | O(log m)     | O(log m + f)          | Same as Put with tombstone        |
| Flush      | O(m log m)   | O(m log m)            | m = MemTable size                 |
| Compaction | O(n log k)   | O(n log k)            | n = entries, k = files            |

### 2.4 Expected Performance Characteristics

| Metric              | Expected Value    | Optimization Factors                 |
| ------------------- | ----------------- | ------------------------------------ |
| Write Throughput    | 100K-500K ops/sec | MemTable size, flush frequency       |
| Read Latency        | 0.1-10ms          | Bloom filter FP rate, cache hit rate |
| Range Query Latency | 1-100ms           | Result size, level count             |
| Space Amplification | 1.1x-2x           | Compaction frequency, merge policy   |
| Write Amplification | 10x-30x           | Size ratio, merge policy             |
| Recovery Time       | 1-30s             | WAL size, level count                |

### 2.5 Pseudocode for Key Operations

#### Put Operation

```
function put(key, value):
    validate(key, value)
    acquire_write_lock(memtable)
    memtable.insert(key, value)
    wal.append(PUT, key, value)
    if memtable.size() >= FLUSH_THRESHOLD:
        schedule_flush()
    release_write_lock(memtable)
```

#### Get Operation

```
function get(key):
    validate(key)

    // Check MemTable
    value = memtable.find(key)
    if value exists and not tombstone:
        return value

    // Check immutable MemTables
    for table in immutable_memtables:
        value = table.find(key)
        if value exists and not tombstone:
            return value

    // Check SSTables
    for level in levels:
        for sstable in level.tables_with_key_in_range(key):
            if sstable.bloom_filter.may_contain(key):
                block = sstable.find_block(key)
                value = block.find(key)
                if value exists and not tombstone:
                    return value

    return KEY_NOT_FOUND
```

#### Range Query Operation

```
function range(start_key, end_key):
    validate(start_key, end_key)
    results = []

    // Collect from MemTable
    memtable_results = memtable.range(start_key, end_key)
    results.merge(memtable_results)

    // Collect from immutable MemTables
    for table in immutable_memtables:
        table_results = table.range(start_key, end_key)
        results.merge(table_results)

    // Collect from SSTables
    for level in levels:
        level_results = []
        for sstable in level.tables_with_range_overlap(start_key, end_key):
            sstable_results = sstable.range(start_key, end_key)
            level_results.merge(sstable_results)
        results.merge(level_results)

    // Remove duplicates and tombstones
    return results.deduplicate().remove_tombstones()
```

#### Compaction Operation

```
function compact(level, files):
    // Select files to compact
    source_files = select_files_for_compaction(level, files)
    target_level = level + 1

    // Find overlapping files in target level
    target_files = find_overlapping_files(target_level, source_files)

    // Create iterators for all files
    iterators = create_iterators(source_files, target_files)

    // Merge iterators
    merger = create_merger(iterators)

    // Create new SSTable builder
    builder = create_sstable_builder(target_level)

    // Process all entries
    while merger.has_next():
        entry = merger.next()
        if is_latest_version(entry) and not is_deleted(entry):
            builder.add(entry)

    // Finalize new SSTables
    new_files = builder.finalize()

    // Update manifest atomically
    update_manifest(source_files, target_files, new_files)

    // Delete old files
    delete_files(source_files, target_files)
```

## 3. Implementation Plan

The implementation will proceed in the following phases:

1. **Core Infrastructure**:

   - Basic data structures (Skip List, Bloom filters)
   - File formats and serialization
   - Logging and error handling

2. **Basic Operations**:

   - MemTable implementation
   - Simple flush mechanism
   - Basic read/write operations

3. **On-Disk Structure**:

   - SSTable format
   - Level management
   - Simple compaction

4. **Advanced Features**:

   - Optimized Bloom filters
   - Fence pointers
   - Adaptive merge policies

5. **Client-Server Architecture**:

   - DSL parser
   - Network communication
   - Multi-client support

6. **Testing and Optimization**:
   - Unit and integration tests
   - Performance benchmarking
   - Tuning and optimization

This phased approach allows for incremental development and testing, with each phase building on the previous one.
