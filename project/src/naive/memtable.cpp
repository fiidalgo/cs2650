#include "naive/memtable.h"

namespace naive
{

    //--------------------------------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------------------------------
    MemTable::MemTable()
    // No explicit initialization needed for std::map - it's default constructed empty
    {
        // Nothing to do here - the map is already initialized empty
    }

    //--------------------------------------------------------------------------------------------------
    // put() - Insert or update a key-value pair
    //--------------------------------------------------------------------------------------------------
    void MemTable::put(MemTable::Key key, MemTable::Value value)
    {
        // Create an optional value (not null) and insert/update in the map
        data_[key] = value;
    }

    //--------------------------------------------------------------------------------------------------
    // get() - Retrieve the value for a key
    //--------------------------------------------------------------------------------------------------
    std::optional<MemTable::Value> MemTable::get(MemTable::Key key) const
    {
        // Try to find the key in the map
        auto it = data_.find(key);

        // If key not found, return nullopt
        if (it == data_.end())
        {
            return std::nullopt;
        }

        // Return the value (which might be nullopt if this is a tombstone)
        return it->second;
    }

    //--------------------------------------------------------------------------------------------------
    // remove() - Mark a key as deleted by setting its value to nullopt (tombstone)
    //--------------------------------------------------------------------------------------------------
    bool MemTable::remove(MemTable::Key key)
    {
        // Find the key
        auto it = data_.find(key);

        // If key doesn't exist, return false (nothing to delete)
        if (it == data_.end())
        {
            return false;
        }

        // Mark as deleted by setting value to nullopt (tombstone)
        it->second = std::nullopt;
        return true;
    }

    //--------------------------------------------------------------------------------------------------
    // range() - Get all key-value pairs in a specified range
    //--------------------------------------------------------------------------------------------------
    std::vector<std::pair<MemTable::Key, MemTable::Value>> MemTable::range(MemTable::Key start_key, MemTable::Key end_key) const
    {
        std::vector<std::pair<MemTable::Key, MemTable::Value>> result;

        // Find the first key >= start_key
        auto it = data_.lower_bound(start_key);

        // Iterate until we reach end_key or the end of the map
        while (it != data_.end() && it->first < end_key)
        {
            // Only include entries that have a value (not tombstones)
            if (it->second.has_value())
            {
                result.emplace_back(it->first, it->second.value());
            }
            ++it;
        }

        return result;
    }

    //--------------------------------------------------------------------------------------------------
    // size() - Get the number of entries in the MemTable
    //--------------------------------------------------------------------------------------------------
    size_t MemTable::size() const
    {
        return data_.size();
    }

    //--------------------------------------------------------------------------------------------------
    // empty() - Check if the MemTable is empty
    //--------------------------------------------------------------------------------------------------
    bool MemTable::empty() const
    {
        return data_.empty();
    }

    //--------------------------------------------------------------------------------------------------
    // for_each() - Apply a function to each entry in the MemTable
    //--------------------------------------------------------------------------------------------------
    void MemTable::for_each(const std::function<void(MemTable::Key, const std::optional<MemTable::Value> &)> &func) const
    {
        // Iterate through all entries and apply the function
        for (const auto &[key, value] : data_)
        {
            func(key, value);
        }
    }

} // namespace naive