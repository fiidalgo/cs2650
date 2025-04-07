#pragma once

#include <string>
#include <map>
#include <functional>
#include <optional>

namespace lsm {
namespace naive {

/**
 * In-memory sorted structure for the most recent writes.
 * Uses std::map for internal storage for simplicity.
 */
class MemTable {
public:
    MemTable();
    
    /**
     * Insert or update a key-value pair
     * 
     * @param key The key to insert or update
     * @param value The value to associate with the key
     * @return true if the key was updated, false if it was inserted
     */
    bool put(const std::string& key, const std::string& value);
    
    /**
     * Retrieve the value for a key
     * 
     * @param key The key to look up
     * @return The value if found, std::nullopt otherwise
     */
    std::optional<std::string> get(const std::string& key) const;
    
    /**
     * Perform a range query
     * 
     * @param start_key The start of the range (inclusive)
     * @param end_key The end of the range (inclusive)
     * @param callback Function to call for each key-value pair in the range
     */
    void range(const std::string& start_key, const std::string& end_key,
              const std::function<void(const std::string&, const std::string&)>& callback) const;
    
    /**
     * Get the number of entries in the MemTable
     */
    size_t size() const;
    
    /**
     * Get the total size of the MemTable in bytes
     */
    size_t sizeBytes() const;
    
    /**
     * Clear all entries from the MemTable
     */
    void clear();
    
    /**
     * Get iterator to the beginning of the map
     */
    std::map<std::string, std::string>::const_iterator begin() const;
    
    /**
     * Get iterator to the end of the map
     */
    std::map<std::string, std::string>::const_iterator end() const;
    
private:
    std::map<std::string, std::string> data_;
    size_t size_bytes_;
};

} // namespace naive
} // namespace lsm 