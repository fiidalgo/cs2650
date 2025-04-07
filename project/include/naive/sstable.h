#pragma once

#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <functional>
#include <memory>
#include "naive/memtable.h"

namespace lsm {
namespace naive {

/**
 * @brief Sorted String Table (SSTable) implementation for persistent storage of key-value pairs.
 * 
 * Each SSTable file contains:
 * 1. A header with metadata
 * 2. A sorted list of key-value pairs
 * 
 * Format:
 * - Header: {
 *     "count": number of entries,
 *     "timestamp": creation timestamp,
 *     "min_key": smallest key,
 *     "max_key": largest key
 *   }
 * - Data: Sequence of entries, each formatted as:
 *     - key_length (4 bytes)
 *     - key (key_length bytes)
 *     - value_length (4 bytes)
 *     - value (value_length bytes)
 */
class SSTable {
private:
    // Path to the SSTable file
    std::string file_path_;
    
    // Creation timestamp
    uint64_t timestamp_;
    
    // Number of entries
    size_t count_;
    
    // Min and max keys (for range filtering)
    std::string min_key_;
    std::string max_key_;
    
    // Size of the header in bytes
    size_t header_size_;
    
    /**
     * @brief Write the SSTable to disk
     * 
     * @param entries Vector of key-value pairs from the memtable
     */
    void writeSSTable(const std::map<std::string, std::string>& entries);
    
public:
    /**
     * @brief Construct a new SSTable
     * 
     * @param file_path Path to the SSTable file
     * @param timestamp Creation timestamp (if creating a new SSTable)
     */
    explicit SSTable(std::string file_path, uint64_t timestamp = 0);
    
    /**
     * @brief Create a new SSTable from a MemTable
     * 
     * @param data_dir Directory to store the SSTable file
     * @param memtable MemTable to flush
     * @return std::unique_ptr<SSTable> New SSTable instance
     */
    static std::unique_ptr<SSTable> createFromMemTable(const std::string& data_dir, const MemTable& memtable);
    
    /**
     * @brief Load an existing SSTable from disk
     * 
     * @param file_path Path to the SSTable file
     * @return std::unique_ptr<SSTable> SSTable instance
     */
    static std::unique_ptr<SSTable> load(const std::string& file_path);
    
    /**
     * @brief Retrieve a value for a given key
     * 
     * @param key Key to look up
     * @param metadata Optional metadata to update with access statistics
     * @return std::optional<std::string> The value if found, empty optional otherwise
     */
    std::optional<std::string> get(const std::string& key, void* metadata = nullptr) const;
    
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
     * @brief Iterate through all entries in the SSTable
     * 
     * @param callback Function to call for each key-value pair
     */
    void forEachEntry(const std::function<void(const std::string&, const std::string&)>& callback) const;
    
    /**
     * @brief Get the size of the SSTable file in bytes
     * 
     * @return size_t File size
     */
    size_t getSizeBytes() const;
    
    /**
     * @brief Get the timestamp of the SSTable
     * 
     * @return uint64_t Timestamp
     */
    uint64_t getTimestamp() const;
    
    /**
     * @brief Get the number of entries in the SSTable
     * 
     * @return size_t Number of entries
     */
    size_t getCount() const;
    
    /**
     * @brief Get the smallest key in the SSTable
     * 
     * @return const std::string& Smallest key
     */
    const std::string& getMinKey() const;
    
    /**
     * @brief Get the largest key in the SSTable
     * 
     * @return const std::string& Largest key
     */
    const std::string& getMaxKey() const;
    
    /**
     * @brief Delete the SSTable file
     */
    void remove() const;
};

} // namespace naive
} // namespace lsm 