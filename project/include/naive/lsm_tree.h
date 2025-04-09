#ifndef NAIVE_LSM_TREE_H
#define NAIVE_LSM_TREE_H

#include <cstdint>
#include <memory>
#include <vector>
#include <optional>
#include <string>
#include "naive/memtable.h"

namespace naive
{

    // Forward declarations
    class SSTable;

    /**
     * LSM-Tree - Log-Structured Merge Tree implementation
     *
     * This is a simplified naive implementation with just an in-memory component.
     * It will be expanded with persistence and other optimizations later.
     */
    class LSMTree
    {
    public:
        using Key = int32_t;
        using Value = int32_t;

        /**
         * Constructor - Creates an empty LSM-Tree
         * @param data_dir Directory where SSTables will be stored
         */
        explicit LSMTree(const std::string &data_dir);

        /**
         * Destructor
         */
        ~LSMTree();

        /**
         * Inserts or updates a key-value pair
         * @param key The key to insert
         * @param value The value to associate with the key
         */
        void put(Key key, Value value);

        /**
         * Retrieves the value associated with a key
         * @param key The key to look up
         * @return The value if found, std::nullopt otherwise
         */
        std::optional<Value> get(Key key) const;

        /**
         * Deletes a key-value pair
         * @param key The key to delete
         * @return true if the key existed and was deleted, false otherwise
         */
        bool remove(Key key);

        /**
         * Retrieves all key-value pairs with keys in the range [start_key, end_key)
         * @param start_key The inclusive start of the range
         * @param end_key The exclusive end of the range
         * @return A vector of key-value pairs in the range
         */
        std::vector<std::pair<Key, Value>> range(Key start_key, Key end_key) const;

    private:
        // The data directory where SSTables will be stored
        std::string data_dir_;

        // The in-memory component
        MemTable memtable_;

        // SSTables will be added in the future
    };

} // namespace naive

#endif // NAIVE_LSM_TREE_H