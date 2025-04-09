#ifndef NAIVE_MEMTABLE_H
#define NAIVE_MEMTABLE_H

#include <map>        // For std::map, our underlying data structure
#include <cstdint>    // For int32_t
#include <optional>   // For std::optional
#include <vector>     // For std::vector
#include <functional> // For std::function (used in range queries)

namespace naive
{

    /**
     * MemTable - An in-memory component of the LSM-Tree
     *
     * The MemTable stores key-value pairs in sorted order (by key) in memory.
     * It supports basic operations like put, get, and range queries.
     * When the MemTable reaches a certain size, it will be flushed to disk as an SSTable.
     *
     * Design choices:
     * - Using std::map for the sorted map implementation (balanced binary search tree)
     * - Keys and values are int32_t (supporting both positive and negative values)
     * - Tombstones are implemented using an optional value
     */
    class MemTable
    {
    public:
        // Type definitions for clarity
        using Key = int32_t;
        using Value = int32_t;
        using OptionalValue = std::optional<Value>;

        /**
         * Constructor - Creates an empty MemTable
         */
        MemTable();

        /**
         * Inserts a key-value pair into the MemTable
         * If the key already exists, its value will be updated
         *
         * @param key The key to insert
         * @param value The value to associate with the key
         */
        void put(Key key, Value value);

        /**
         * Retrieves the value associated with a key
         *
         * @param key The key to look up
         * @return The value if found, std::nullopt otherwise
         */
        OptionalValue get(Key key) const;

        /**
         * Marks a key as deleted by setting its value to std::nullopt
         * This is a logical deletion (tombstone) - the key remains in the table
         * but is marked as deleted
         *
         * @param key The key to mark as deleted
         * @return true if the key existed and was marked as deleted, false otherwise
         */
        bool remove(Key key);

        /**
         * Retrieves all key-value pairs with keys in the range [start_key, end_key)
         *
         * @param start_key The inclusive start of the range
         * @param end_key The exclusive end of the range
         * @return A vector of key-value pairs in the range
         */
        std::vector<std::pair<Key, Value>> range(Key start_key, Key end_key) const;

        /**
         * Returns the number of entries in the MemTable
         * Note: This includes tombstones (logically deleted entries)
         *
         * @return The number of entries
         */
        size_t size() const;

        /**
         * Checks if the MemTable is empty
         *
         * @return true if empty, false otherwise
         */
        bool empty() const;

        /**
         * Iterates through all entries in the MemTable and applies the given function
         * This is useful for flushing the MemTable to disk
         *
         * @param func The function to apply to each entry (receives key and optional value)
         */
        void for_each(const std::function<void(Key, const OptionalValue &)> &func) const;

    private:
        // The underlying sorted map data structure
        // Note: Using std::optional<Value> to represent tombstones (std::nullopt for deleted entries)
        std::map<Key, OptionalValue> data_;
    };

} // namespace naive

#endif // NAIVE_MEMTABLE_H