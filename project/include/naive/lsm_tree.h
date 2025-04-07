#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <mutex>
#include <functional>
#include "naive/memtable.h"
#include "naive/sstable.h"

namespace lsm {
namespace naive {

/**
 * @brief Metadata returned with get operations
 */
struct GetMetadata {
    size_t sstables_accessed = 0;
    size_t bytes_read = 0;
};

/**
 * @brief Naive LSM-Tree implementation with a MemTable and SSTables.
 * 
 * Features:
 * - In-memory MemTable for fast writes
 * - Periodic flushing of MemTable to SSTable when size limit reached
 * - Multiple immutable SSTables on disk, queried in reverse chronological order
 * - No compaction (all data remains in Level 0)
 */
class LSMTree {
private:
    // Directory to store SSTable files
    std::string data_dir_;
    
    // Size limit for MemTable before flushing to disk
    size_t memtable_size_bytes_;
    
    // Active MemTable for writes
    std::unique_ptr<MemTable> memtable_;
    
    // List of SSTables, sorted by timestamp (newest first)
    std::vector<std::unique_ptr<SSTable>> sstables_;
    
    // Mutex for protecting shared data during operations
    mutable std::mutex mutex_;
    
    /**
     * @brief Load existing SSTable files from the data directory
     */
    void loadExistingSSTables();
    
    /**
     * @brief Flush the current MemTable to an SSTable on disk
     */
    void flushMemTable();
    
public:
    /**
     * @brief Construct a new LSM-Tree
     * 
     * @param data_dir Directory to store SSTable files
     * @param memtable_size_bytes Size limit for MemTable before flushing to disk
     */
    explicit LSMTree(std::string data_dir, size_t memtable_size_bytes = 1024 * 1024);
    
    /**
     * @brief Insert or update a key-value pair
     * 
     * @param key Key to insert/update
     * @param value Value to store
     */
    void put(const std::string& key, const std::string& value);
    
    /**
     * @brief Retrieve a value for a given key
     * 
     * @param key Key to look up
     * @param metadata Optional pointer to store metadata about the get operation
     * @return std::optional<std::string> The value if found, empty optional otherwise
     */
    std::optional<std::string> get(const std::string& key, GetMetadata* metadata = nullptr) const;
    
    /**
     * @brief Perform a range query from start_key to end_key (inclusive)
     * 
     * @param start_key Start of the key range
     * @param end_key End of the key range
     * @param callback Function to call for each key-value pair in the range
     */
    void range(const std::string& start_key, const std::string& end_key,
               const std::function<void(const std::string&, const std::string&)>& callback) const;
    
    /**
     * @brief Delete a key-value pair
     * 
     * In LSM-Trees, deletion is typically implemented as a "tombstone" marker.
     * 
     * @param key Key to delete
     */
    void remove(const std::string& key);
    
    /**
     * @brief Manually flush the MemTable to disk
     */
    void flush();
    
    /**
     * @brief Compaction is not implemented in the naive version
     */
    void compact();
    
    /**
     * @brief Close the LSM-Tree and flush any pending data
     */
    void close();
    
    /**
     * @brief Get the number of SSTables
     * 
     * @return size_t Number of SSTables
     */
    size_t getSStableCount() const;
    
    /**
     * @brief Get the current size of the MemTable in bytes
     * 
     * @return size_t Size in bytes
     */
    size_t getMemTableSize() const;
    
    /**
     * @brief Get the total size of all SSTables in bytes
     * 
     * @return size_t Size in bytes
     */
    size_t getTotalSizeBytes() const;
    
    /**
     * @brief Get statistics about the LSM-Tree
     * 
     * @return std::string JSON string with statistics
     */
    std::string getStats() const;
    
    /**
     * @brief Clear all data (for testing purposes)
     */
    void clear();
    
    /**
     * @brief Destructor
     */
    ~LSMTree();
};

} // namespace naive
} // namespace lsm 