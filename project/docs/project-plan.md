# Midway Check-in LSM-Tree, Experiments, and Future Plan for Phase 2

## 1. How the Current LSM-Tree Works

### 1.1 Overview
My LSM-Tree is a **write-optimized key-value store** that defers on-disk writes by first buffering them in memory. When the in-memory portion gets too large, it is flushed to disk as an immutable file. My LSM-Tree currently does not perform advanced compactions or optimizations like Bloom filters; I just have a single “Level 0” for on-disk files.

### 1.2 In-Memory Storage: The MemTable
- **Skip List Structure**: The MemTable is basically a skip list. A skip list is like a linked list but better: each node has multiple “forward pointers” that let you skip many elements at once, making insert/lookup faster than a plain linked list.
- **Insertions**: When you `put(key, value)`, the code inserts `(key, value, deleted=false)` into the skip list. If the key already exists there, the old value is replaced.
- **Deletions**: A `delete(key)` works the same way, but `deleted=true` is set as a tombstone. That way it's clear that the key is “logically removed.”
- **Size Threshold**: I track how many entries or total memory the MemTable uses. If it grows beyond 4MB or 1 million entries (whichever comes first), it is marked `immutable`. An immutable MemTable can no longer be changed.

### 1.3 Flushing MemTable to an SSTable
- **Background Thread**: When a MemTable becomes immutable, the background thread eventually writes it to disk as a Sorted String Table (SSTable). While it’s flushing, a brand-new MemTable is created to keep servicing new writes immediately.
- **SSTable Format**: Each SSTable on disk has:
  - A small **header**: format version, how many items are in it, the minimum key, the maximum key.
  - A **body**: each entry is `(key, value, is_deleted)`. It’s sorted by key.
- **Level 0 Only**: Right now, all SSTables end up in “Level 0.” I do not yet merge older SSTables further down or combine them.

### 1.4 On-Disk Reads
- **Get Queries**:
  1. Look in the active MemTable (the skip list in memory).
  2. Look in any immutable MemTables that haven’t been flushed yet (or are flushing).
  3. Look through the SSTables on disk in reverse chronological order (i.e., newest file first).
     - **Linear Search**: Currently, read from start to finish in that SSTable file until you either find the matching key or surpass it.
  - If you find a `deleted=true` tombstone, that means the key doesn’t exist.
- **Range Queries**:
  - Gather all keys in [start, end) from the active MemTable, then from each immutable MemTable, then from each SSTable.
  - Combine them, sort them by key, and remove duplicates so that the newest version of each key is kept.

### 1.5 Basic Architecture Recap
1. New writes go into an in-memory skip list (MemTable).
2. If that skip list grows too large, it’s flushed to disk as an SSTable in Level 0.
3. Reads check memory first, then scan disk files in order to find the requested data.
4. Currently, I do not do multi-level compaction or bloom filters, so there are higher read costs as more SSTables are accumulated.

---

## 2. How the Experiments Work

I've done two major baseline experiments using Python scripts:

### 2.1 PUT Experiment
- **Script**: `experiment_put.py`
- **Procedure**:
  1. **Generate a file** of `(key, value)` lines for a certain number of records (e.g., 100k, 200k, 500k, 1M).
  2. **Start LSM-Tree server** with an empty data folder.
  3. **Send a `load` command** to the server pointing to the generated file. That triggers the system to do a bunch of `put` operations internally.
  4. Once loading finishes, the script queries the server for stats (to see read/write operations, etc.).
  5. **Record the time** it took to do all the puts and the I/O usage (read/writes, bytes).
  6. **Repeat** for different record counts, storing results in a CSV.

- **Goal**: Measure how the LSM-Tree ingestion time scales as you increase the data size, and how many bytes of I/O are written.

### 2.2 GET Experiment
- **Script**: `experiment_get.py`
- **Procedure**:
  1. Start the LSM-Tree server with an **already loaded** dataset of around 1 million keys (from prior runs, or loaded once).
  2. For each run, pick a certain number of random `get` queries (e.g. 1k, 2k, 5k, 10k).
  3. Measure the total time it takes to process all those GETs. 
  4. Record the read I/Os. 
  5. Results get appended to another CSV.

- **Goal**: Determine how read time and read I/Os scale with the number of random queries. Currently linear scans in multiple SSTables, so read amplification can get large.

---

## 3. Step-by-Step Project Plan for the Remaining Work

Below is a plan aligned with the **project specifications** (including the final deliverable of a working LSM-tree, the final report with experiments on each operator, and 3 additional optimizations).

### 3.1 Optimization 1: Implement Multi-Level Compaction
**What**: Instead of keeping all SSTables in Level 0, introduce multiple levels (L0, L1, L2, ...) with a size ratio. When L0 accumulates too many files, or too much data, merge them into L1, and so on.

1. **Design**:
   - Pick a compaction policy (like size-tiered or level-tiered).
   - For example, if L1 is allowed to be 10x bigger than L0, once L0’s total size is ~X, pick a run of files from L0 and merge them into a new set of files in L1.
2. **Implementation**:
   - Add code to track how many SSTables or how many bytes are in L0.
   - When thresholds are exceeded, do a sort merge of the L0 data with L1 data that has overlapping keys. Write out new sorted SSTables for L1, remove the old ones from L0 and L1.
3. **Benefit**:
   - This will drastically reduce the number of separate SSTables. `get` queries will have fewer files to scan, especially if older data keeps consolidating in deeper levels.

**Experiment**:
- Re-run GET queries after compaction is done. Should see fewer I/Os for point lookups because data is less scattered.

### 3.2 Optimization 2: Add Bloom Filters to Each SSTable
**What**: A Bloom filter is a space-efficient data structure that helps you quickly decide whether a key cannot be in a file, avoiding needless disk reads.

1. **Design**:
   - When you flush or compact data to form an SSTable, build a Bloom filter for all keys in that table. Store it on disk (or partially in memory if you like).
   - On a `get(key)`, check the Bloom filter first. If it says “key not present,” skip that SSTable altogether.
2. **Implementation**:
   - Implement a classic Bloom filter: choose the filter size, number of hash functions, etc.
   - Integrate it in the read path. If the filter returns “definitely not present,” do not linear-scan that SSTable’s data.
3. **Benefit**:
   - Cut out many unnecessary file scans, especially when the key truly isn’t in that table, drastically lowering read cost.

**Experiment**:
- Compare read performance and I/O with Bloom filters vs. without. Show that for random queries that do not exist in the SSTable, skip scanning and gain speed.

### 3.3 Optimization 3: Implement Fence Pointers for Faster On-Disk Search
**What**: Fence pointers store partial indexing within each SSTable, typically storing the min key for each data block (or some chunk). You can then binary-search these pointers to jump near your key and do a narrower search on disk.

1. **Design**:
   - Decide on block size (e.g., 4KB or 8KB).
   - For every block, record the minimum key in that block. Keep these min keys in a small, in-memory array or separate structure. 
   - On a `get(key)`, do a binary search on fence pointers to find the right block, then read only that block (or a small range of blocks).
2. **Implementation**:
   - Adjust SSTable format to break data into blocks. Possibly store the fence pointers in the file’s header or a separate index region at the end.
   - On reading, do the pointer-based search instead of scanning everything linearly.
3. **Benefit**:
   - With fence pointers, the time to locate a key is reduced from linear in the entire file to linear in the block size. 

**Experiment**:
- Show improved read times and reduced I/O compared to full-file scans. Possibly combine with Bloom filters for even better performance.

### 3.4 Concurrency

**Goal**: Allow background flushes and compactions to run in parallel with user-facing queries (GET, PUT, DELETE, RANGE). Possibly run multiple merges/flushes at once if hardware permits.

#### 2.4.1 Concurrency Architecture

1. **Active/Immutable MemTable Locking**  
   - **Design**: Use a lightweight mutex to protect transitions from an active MemTable to an immutable one.  
   - **Implementation**: 
     - When the MemTable crosses the threshold, the main thread marks it immutable and spawns a flush task. 
     - A fresh active MemTable is allocated immediately, so user PUTs don’t block for the flush.

2. **SSTable Manifest (Versioning)**  
   - **Design**: Maintain a “manifest” or “version” object that describes all levels, files, and metadata at a point in time.  
   - **Implementation**: 
     - Each time a flush or compaction finishes writing new files, it updates the manifest atomically. 
     - Older files are marked “obsolete” but remain accessible to queries that started under the previous version. 
     - Queries pick up the newest manifest version available at the time the query starts.

3. **Thread Pool for Background Tasks**  
   - **Design**: Create a pool of worker threads (e.g., sized to CPU cores) that can accept tasks for flushing or compaction.  
   - **Implementation**:
     1. **Task Queue**: A shared queue of tasks (e.g. “flush MemTable M,” “compact L0 → L1,” etc.). 
     2. **Workers**: Each worker thread loops, pulling tasks and executing them. 
     3. **Synchronization**: 
        - Use condition variables or semaphores to notify threads when new tasks arrive.  
        - Use a global mutex for the queue itself.

4. **Locking Strategy**  
   - For compactions, you typically need read access to the old files and write access to the new ones.  
   - For queries, you want to read a stable snapshot of which files are valid.  
   - **Implementation**:  
     - A small lock for modifying the manifest.  
     - A “copy-on-write” or “versioning” approach so queries see a stable list of files.  
     - The actual on-disk merges happen in the background worker. It merges data into new files, then does an atomic pointer swap to update the manifest.

#### 2.4.2 Concurrency Benefits and Pitfalls

- **Benefits**: 
  - Flushes and compactions no longer stall user queries; throughput can increase significantly on multi-core systems. 
  - The main server thread can quickly handle new `put` or `get` commands while background merges proceed.

- **Pitfalls**: 
  - Implementation complexity is higher (more locks, potential for race conditions). 
  - Must ensure a consistent read snapshot and that old files aren’t deleted until no queries are using them.

#### 2.4.3 Concurrency Testing
1. **Stress Tests**: Run large-scale inserts while also issuing random GET queries to confirm that queries remain correct and don’t time out.  
2. **Failure Scenarios**: If a flush or compaction fails, ensure the system can recover gracefully (e.g., ignore partial merges).


### 3.5 Final Integration, Testing, and Performance Experiments
1. **Full Testing**: Ensure correctness of put/get/delete/range across all levels. 
2. **Performance**: 
   - Provide final metrics on read throughput, write throughput, and space usage. 
   - Evaluate each major optimization from Steps 1, 2, and 3 in isolation (compaction alone, compaction + Bloom, compaction + Bloom + fence pointers, etc.).
   - Analyze concurrency improvements (if implemented).
3. **Final Report**: 
   - According to specs, you’ll produce ~10 pages including design, final performance graphs, and analysis of different workloads (read-heavy, mixed, write-heavy).
   - Demonstrate that the system meets the **basic** and **additional** design requirements.

---

### Summary of Steps and Timelines
- **Now–April 4**: Optimization 1 (Multi-level compaction)
- **April 5–April 12**: Optimization 2 (Bloom filters)  
- **April 13–April 19**: Optimization 3 (Fence pointers)
- **April 19–May 2**: Concurrency
- **May 3–Deadline**: Integration, experiments, final paper