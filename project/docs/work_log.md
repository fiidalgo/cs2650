# Work Log

This document tracks the development progress of this project. It includes details on progress, implementation decisions, and future plans.

## 2025-04-08

### Completed

- Cleaned up the repository structure and set up the project organization
- Created a detailed project plan outlining the step-by-step implementation approach
- Designed the overall architecture as a client-server system
- Defined the project's progression through phases: naive implementation → compaction → bloom filters → fence pointers → concurrency
- Established the DSL for interacting with the database
- Designed the testing framework and metrics collection approach

### Design Decisions

#### LSM-Tree Architecture

The LSM-Tree (Log-Structured Merge Tree) design was chosen because it offers excellent write performance while maintaining reasonable read performance.

1. **MemTable**: Using a balanced tree (`std::map`) rather than a skip list for the initial implementation. This simplifies development while still providing $O(\log n)$ operations. Skip lists could be considered later for potential performance improvements.

2. **SSTable Format**: Designed with a simple format initially (keys sorted, values adjacent) to facilitate easier debugging and implementation. The format will evolve as optimizations are added.

3. **Merge Policy**: Selected leveling as the default compaction strategy (over tiering) because:

   - Provides better read performance due to fewer SSTables to search
   - Simpler to reason about initially
   - More widely used in production systems (e.g., LevelDB, RocksDB)

4. **Client-Server Architecture**: The two-process design (separate server and client) provides:
   - Clean separation of concerns between storage engine and user interface
   - Support for network-based access and multiple clients
   - More realistic deployment structure that mirrors production database systems

#### Testing Framework Approach

Designed a comprehensive testing framework to:

- Generate various workloads (read-heavy, write-heavy, range-heavy, mixed)
- Collect detailed metrics (latency, throughput, I/O statistics)
- Support hyperparameter tuning with grid search
- Visualize performance trends

This approach will allow data-driven optimization decisions rather than guessing.

### Optimization Explanations

1. **Compaction**: Process of merging multiple SSTables to control their count and organization. Key to maintaining read performance as the dataset grows.

   - Leveling: Organizes SSTables into levels; each level is K times larger than the one above
   - Tiering: Groups SSTables into tiers; compaction happens when a tier fills up

2. **Bloom Filters**: Probabilistic data structures that can quickly tell if a key is definitely not in a set. In LSM-Trees, they allow skipping SSTables that don't contain a queried key, dramatically improving read performance.

3. **Fence Pointers**: Index structures that divide SSTables into blocks and store the minimum key for each block. Allow binary search to quickly locate the correct block for a key, avoiding scanning the entire SSTable.

4. **Concurrency Handling**: Techniques to allow multiple threads to safely access the LSM-Tree simultaneously, improving throughput on multi-core systems.

### Next Steps

1. Set up the basic project structure (directories, CMakeLists.txt)
2. Implement the core MemTable data structure
3. Implement the SSTable format and I/O operations
4. Create the naive LSM-Tree implementation (put, get, range, delete operations)
5. Begin work on the server component to handle client connections

## 2025-04-09
