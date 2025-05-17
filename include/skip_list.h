#ifndef SKIP_LIST_H
#define SKIP_LIST_H

#include <cstdint>
#include <memory>
#include <random>
#include <vector>
#include <optional>
#include <mutex>
#include "lsm_tree.h"
#include "constants.h"

namespace lsm
{

    // Node in the skip list
    class SkipListNode
    {
    public:
        SkipListNode(int64_t key, int64_t value, int height);

        // Special constructor for sentinel nodes
        explicit SkipListNode(int height);

        int64_t get_key() const;
        int64_t get_value() const;
        void set_value(int64_t new_value);

        // Get the next node at a specific level
        SkipListNode *next(int level) const;

        // Set the next node at a specific level
        void set_next(int level, SkipListNode *node);

        // Get the height of this node
        int get_height() const;

    private:
        int64_t key;
        int64_t value;
        int height;
        std::vector<SkipListNode *> next_nodes;
    };

    // Skip list implementation for the buffer
    class SkipList
    {
    public:
        SkipList();
        ~SkipList();

        // Deleted copy/move constructors and assignment operators
        SkipList(const SkipList &) = delete;
        SkipList &operator=(const SkipList &) = delete;
        SkipList(SkipList &&) = delete;
        SkipList &operator=(SkipList &&) = delete;

        // Insert or update a key-value pair
        void insert(int64_t key, int64_t value);

        // Get the value for a key
        std::optional<int64_t> get(int64_t key) const;

        // Get all key-value pairs in a range [start_key, end_key)
        std::vector<KeyValuePair> range(int64_t start_key, int64_t end_key) const;

        // Check if the buffer is full
        bool is_full() const;

        // Get the current size in bytes
        size_t size_bytes() const;

        // Get the number of elements in the skip list
        size_t element_count() const;

        // Empty the skip list
        void clear();

        // Get all key-value pairs in sorted order
        std::vector<KeyValuePair> get_all_sorted() const;

    private:
        // Head and tail sentinel nodes
        std::unique_ptr<SkipListNode> head;
        std::unique_ptr<SkipListNode> tail;

        // Current size in bytes
        size_t current_size;

        // Number of elements
        size_t num_elements;

        // For synchronization
        mutable std::mutex skip_list_mutex;

        // Random number generator for levels
        std::mt19937 rng;

        // Determine the height for a new node
        int random_height();

        // Find the nodes that would precede a key at each level
        void find_predecessors(int64_t key, std::vector<SkipListNode *> &predecessors) const;

        // Estimate the size of a key-value pair in bytes
        size_t estimate_pair_size(int64_t key, int64_t value, int height) const;
    };

} // namespace lsm

#endif // SKIP_LIST_H