# LSM-Tree Key-Value Store Implementation Plan

## Project Goal
The goal of the project is to build a modern write-optimized NoSQL key-value store. The system will be based on a state-of-the-art variatn of LSM-tree (Log Structured Merge tree) design. There are two parts in the project. In the first part, you will build a basic single thread key-value store that will include all major design components as in state-of-the-art key-value stores. In the second part, you will extend the design to support multiple threads and be able to process multiple queries in parallel, while utilizing all cores of a modern server machine. 

## Project Description
The project is divided into two parts. The first part is about designing the basic structure of an LSM-tree for reads and writes, while the second part is about designing and implementing the same functionality in a parallel way so we can support multiple concurrent reads and writes. Each part will have a first milestone: a design document. Here you will discuss your design and development strategy. Template can be found in `./docs/design-doc-template.pdf`. The minimum design should align with the LSM-tree design specifications mentioned in Monkey, which can be found in `./docs/monkey.pdf`, including the following:
* Every level can follow either of the following merge policies: leveling, tiering, or lazy leveling.
* Each level includes bloom filter(s) with optimized bits per entry to determine if a key is not contained in the level.
* Each level includes fence pointers to allow page or block access within a run.
* You may alter the design (e.g. customizing merge policies, varying lboom filter sizes, etc.) in an effort to achieve faster reads or faster writes, but they should not revert to simpler designs (e.g. making a level just a plain array).
* The workload generator that you would be using can generate negative keys and values as well. So you need to support both signed and unsigned key-value pairs.
* The system should run as two parts. First is a key-value database server which manages the data and waits to receive queries. Second is a lightweight query client which can communicate queries with said server. Multiple clients should be able to connect to the key-value database server.
* The database server will respond to queries that adhere to the Domain Specific Language (DSL) which can be found in `./docs/systems-project-specs.pdf`. This allows it to behave as a bare bones key-value store. 
* The database server should persist data to a specified data directory.
* If the data directory has contents at startup, the system should load any relevant data in, preparing or reconstructing any additional structures if necessary, for instance, fence pointers, Bloom filters, etc.

Note: During compaction, as the number of levels increase, merging the content of the deeper levels of the tree will demand more memory. An alternative is to use external sorting in which data is brought into memory in chunks, sorted, and written to a temporary file. Then, the sorted files are merged and written back to disk. However, this process is still slow due to high I/O cost of moving the entire data (residing at the two levels) to memory and writing to disk. A commonly used approach is a partial compaction in which one or more files of level-L and level-(L+1) are removed. This reduces the overall data movement cost thereby being significantly faster and less memory-intensive.

**Additional Design Considerations:**

Additionally, you should describe, implement, and evaluate at least three different optimizations that support the performance requirements. The purpose of the optimizations is to approach improving the system design and performance from both a theoretical and empirical approach. These may include but are not limited to altering the size ratio between the levels, the choice of the buffer data structure design, whether each level will consist of one or more structures (e.g. sorted arrays), how merging happens and when it is triggered, and many more design decisions that are open to you.

## Suggested Timeline
1. Familiarize with LSM-Trees and variations
1. Design document: Design of basic LSM-Tree and development plan, initial testing and development
1. Development of basic LSM-Tree
1. Design document: Describe plan for parallelization
1. Refining LSM-Tree and parallelization
1. finalizing development of parallel LSM-Tree

## Step 1: Understand LSM-Tree Fundamentals
- [ ] Study the basic structure and concepts of LSM-trees
- [ ] Research existing implementations (LevelDB, RocksDB, Cassandra)
- [ ] Understand core operations (writes, reads, deletes, merges)
- [ ] Learn about Bloom filters and fence pointers
- [ ] Research merge policies (leveling, tiering, lazy leveling)

## Step 2: Understand Performance Characteristics
- [ ] Study read vs. write tradeoffs in LSM-trees
- [ ] Research size ratio optimization between levels
- [ ] Understand space amplification issues
- [ ] Research merge/compaction strategies

## Step 3: Review Project Requirements
- [ ] Review the DSL specifications
- [ ] Understand workload generator and test framework
- [ ] Study required API endpoints (`put`, `get`, `range`, `delete`, `load`, `print stats`)

## Step 4: Design Document - Basic LSM-Tree

### Step 4.1: Design In-Memory Buffer (L0)
- [ ] Choose appropriate data structure (skiplist, red-black tree, B+ tree)
- [ ] Define memory layout and organization
- [ ] Design buffer flushing mechanism
- [ ] Plan for buffer size thresholds

### Step 4.2: Design On-Disk Components
- [ ] Define file format for persisted runs
- [ ] Design level organization with merge thresholds
- [ ] Plan run structure and key range management
- [ ] Design file management system

### Step 4.3: Design Core Operations
- [ ] Design write path (put, update, delete)
- [ ] Design read path (get, range)
- [ ] Design merge/compaction process
- [ ] Plan for delete marker handling

### Step 4.4: Design Optimization Components
- [ ] Design Bloom filter implementation with size optimization
- [ ] Design fence pointers for efficient searches
- [ ] Plan for at least three performance optimizations
  - Possible options:
    - Variable Bloom filter bits per level
    - Optimized merge policy selection per level
    - Partial compaction strategies
    - Smart level size ratio configuration
    - Caching strategies

### Step 4.5: Document Design Decisions
- [ ] Create diagram of overall architecture
- [ ] Document data structures and algorithms
- [ ] Define complexity analysis for operations
- [ ] Document expected performance characteristics
- [ ] Create pseudocode for key operations

## Step 5: Implementation of Basic LSM-Tree Design

### Step 5.1: Set Up Project Infrastructure
- [ ] Create project structure and build system
- [ ] Set up testing framework
- [ ] Implement basic logging and debugging infrastructure
- [ ] Create client-server architecture as specified

### Step 5.2: Implement In-Memory Buffer
- [ ] Implement chosen data structure for buffer
- [ ] Implement buffer operations (insert, lookup, delete)
- [ ] Add support for buffer size tracking
- [ ] Implement buffer flush mechanism

### Step 5.3: Implement Disk Components
- [ ] Create file format for persisted runs
- [ ] Implement serialization/deserialization
- [ ] Create level management system
- [ ] Implement catalog and manifest for file tracking

### Step 5.4: Implement Core Operations
- [ ] Implement put operation
- [ ] Implement get operation
- [ ] Implement range query operation
- [ ] Implement delete operation with tombstone markers
- [ ] Implement load operation for bulk loading

### Step 5.5: Implement Optimizations
- [ ] Implement Bloom filters
- [ ] Implement fence pointers
- [ ] Implement merging/compaction logic
- [ ] Add the three planned performance optimizations

### Step 5.6: Implement DSL Interface
- [ ] Create parser for command syntax
- [ ] Implement command handlers
- [ ] Add stats command functionality
- [ ] Set up client-server communication

### Step 5.7: Testing and Validation
- [ ] Create unit tests for components
- [ ] Develop integration tests
- [ ] Test with workload generator
- [ ] Validate correctness with various workloads
- [ ] Benchmark performance

## Step 6: Design Document - Parallelization
- [ ] Design concurrency model
- [ ] Plan thread management
- [ ] Design concurrent access to buffer
- [ ] Design concurrent merging strategies
- [ ] Plan for synchronization mechanisms
- [ ] Document expected performance scaling

## Step 7: Implementation of Parallel LSM-Tree

### Step 7.1: Implement Concurrency Infrastructure
- [ ] Add thread pool management
- [ ] Implement read-write locks
- [ ] Set up thread-safe data structures
- [ ] Create synchronization primitives

### Step 7.2: Implement Concurrent Operations
- [ ] Make buffer thread-safe
- [ ] Implement concurrent reads
- [ ] Implement concurrent writes
- [ ] Handle concurrent merges/compactions

### Step 7.3: Implement Optimizations for Concurrency
- [ ] Add lock-free optimizations where possible
- [ ] Implement partitioning strategies
- [ ] Optimize for multi-core scaling
- [ ] Balance read/write performance under concurrency

### Step 7.4: Testing and Validation
- [ ] Test with multiple concurrent clients
- [ ] Validate correctness under concurrency
- [ ] Perform stress testing
- [ ] Measure scaling with number of cores
- [ ] Benchmark with different workload patterns

## Step 8: Finalization and Optimization

### Step 8.1: Performance Tuning
- [ ] Profile code for bottlenecks
- [ ] Optimize critical paths
- [ ] Fine-tune parameters
- [ ] Calibrate to meet performance targets

### Step 8.2: Prepare Final Deliverables
- [ ] Complete implementation with documentation
- [ ] Prepare final report
  - Abstract
  - Introduction
  - Design section
  - Experimental section
  - Conclusion
- [ ] Create visualization tools for demonstration
- [ ] Prepare demo script

### Step 8.3: Conduct Experiments
- [ ] Test with varying data sizes (100MB-10GB)
- [ ] Test with different data distributions
- [ ] Evaluate read vs. write ratios
- [ ] Test with different buffer sizes
- [ ] Evaluate level ratio impacts
- [ ] Measure multi-threading scaling
- [ ] Test with varying client counts
- [ ] Document all findings

## Detailed Implementation Checklist for Core Components

### In-Memory Buffer (L0)
- [ ] Choose data structure (e.g., skiplist)
- [ ] Implement key-value insertion
- [ ] Implement key lookup
- [ ] Implement range scan
- [ ] Add size tracking
- [ ] Implement flush threshold detection
- [ ] Create serialization for disk persistence

### On-Disk Structure
- [ ] Define file format with headers
- [ ] Implement run creation from buffer
- [ ] Add key range tracking per file
- [ ] Implement file management
- [ ] Create catalog for tracking runs and levels
- [ ] Implement manifest for recovery

### Merge/Compaction
- [ ] Implement trigger conditions
- [ ] Create merge policy implementations
  - [ ] Leveling (one run per level)
  - [ ] Tiering (multiple runs per level)
  - [ ] Lazy leveling (hybrid approach)
- [ ] Implement sort-merge algorithm
- [ ] Handle tombstone markers during merge
- [ ] Implement background merging

### Optimizations
- [ ] Bloom filters
  - [ ] Implement filter creation
  - [ ] Optimize bits-per-entry
  - [ ] Add level-specific tuning
- [ ] Fence pointers
  - [ ] Implement for run partitioning
  - [ ] Optimize memory usage
- [ ] Custom optimizations (implement at least three)
  - [ ] Optimization 1: _____
  - [ ] Optimization 2: _____
  - [ ] Optimization 3: _____

### Parallel Processing
- [ ] Thread pool for background operations
- [ ] Concurrent buffer access
- [ ] Parallel merge implementation
- [ ] Read/write concurrency control
- [ ] Synchronization for catalog updates

### Client-Server Architecture
- [ ] Implement server listener
- [ ] Create client connection handling
- [ ] Implement request parsing
- [ ] Set up response formatting
- [ ] Add multi-client support

### CS265 DSL Implementation
- [ ] Put command (`p [key] [value]`)
- [ ] Get command (`g [key]`)
- [ ] Range command (`r [start] [end]`)
- [ ] Delete command (`d [key]`)
- [ ] Load command (`l [filepath]`)
- [ ] Stats command (`s`)

## Experimental Evaluation Plan

### Performance Metrics
- [ ] Throughput (operations per second)
- [ ] Latency (ms per operation)
- [ ] I/O counts (reads/writes)
- [ ] Cache behavior
- [ ] Space amplification
- [ ] Write amplification

### Experiment Dimensions
- [ ] Data size (100MB to 10GB)
- [ ] Data distribution (uniform vs skewed)
- [ ] Query distribution (uniform vs skewed)
- [ ] Read-write ratio (10:1 to 1:10)
- [ ] Buffer size (4K to 100MB)
- [ ] Level size ratio (2 to 10)
- [ ] Thread count (1 to max cores)
- [ ] Client count (1 to 64)

### Analysis Tasks
- [ ] Generate graphs for each dimension
- [ ] Analyze tradeoffs between configurations
- [ ] Document optimization effects
- [ ] Measure scaling behavior
- [ ] Compare merge policies
- [ ] Evaluate overall system performance