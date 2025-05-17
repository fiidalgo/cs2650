#include "../include/skip_list.h"
#include <random>
#include <limits>
#include <algorithm>
#include <optional>

namespace lsm
{

    // SkipListNode implementation

    SkipListNode::SkipListNode(int64_t key, int64_t value, int height)
        : key(key), value(value), height(height), next_nodes(height, nullptr)
    {
    }

    SkipListNode::SkipListNode(int height)
        : key(0), value(0), height(height), next_nodes(height, nullptr)
    {
    }

    int64_t SkipListNode::get_key() const
    {
        return key;
    }

    int64_t SkipListNode::get_value() const
    {
        return value;
    }

    void SkipListNode::set_value(int64_t new_value)
    {
        value = new_value;
    }

    SkipListNode *SkipListNode::next(int level) const
    {
        if (level < 0 || level >= height)
        {
            return nullptr;
        }
        return next_nodes[level];
    }

    void SkipListNode::set_next(int level, SkipListNode *node)
    {
        if (level < 0 || level >= height)
        {
            return;
        }
        next_nodes[level] = node;
    }

    int SkipListNode::get_height() const
    {
        return height;
    }

    // SkipList implementation

    SkipList::SkipList()
        : current_size(0), num_elements(0), rng(std::random_device()())
    {

        // Create sentinel nodes
        head = std::make_unique<SkipListNode>(constants::MAX_SKIP_LIST_HEIGHT);
        tail = std::make_unique<SkipListNode>(constants::MAX_SKIP_LIST_HEIGHT);

        // Connect head to tail at all levels
        for (int i = 0; i < constants::MAX_SKIP_LIST_HEIGHT; ++i)
        {
            head->set_next(i, tail.get());
        }
    }

    SkipList::~SkipList()
    {
        // No need for explicit cleanup due to unique_ptr usage
    }

    void SkipList::insert(int64_t key, int64_t value)
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);

        // Find nodes that would precede the key at each level
        std::vector<SkipListNode *> predecessors(constants::MAX_SKIP_LIST_HEIGHT);
        find_predecessors(key, predecessors);

        // Check if key already exists
        SkipListNode *next_node = predecessors[0]->next(0);
        if (next_node != tail.get() && next_node->get_key() == key)
        {
            // Update existing key's value
            next_node->set_value(value);
            return;
        }

        // Determine height for the new node
        int height = random_height();

        // Calculate size of the new pair
        size_t pair_size = estimate_pair_size(key, value, height);

        // Create new node
        auto new_node = new SkipListNode(key, value, height);

        // Insert the node at each level
        for (int i = 0; i < height; ++i)
        {
            new_node->set_next(i, predecessors[i]->next(i));
            predecessors[i]->set_next(i, new_node);
        }

        // Update size and count
        current_size += pair_size;
        num_elements++;
    }

    std::optional<int64_t> SkipList::get(int64_t key) const
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);

        // Start at the highest level and work down
        SkipListNode *current = head.get();

        for (int level = constants::MAX_SKIP_LIST_HEIGHT - 1; level >= 0; --level)
        {
            // Traverse the current level as far as possible
            while (current->next(level) != tail.get() && current->next(level)->get_key() < key)
            {
                current = current->next(level);
            }
        }

        // Move to the next node at level 0
        current = current->next(0);

        // Check if we found the key
        if (current != tail.get() && current->get_key() == key)
        {
            return current->get_value();
        }

        // Key not found
        return std::nullopt;
    }

    std::vector<KeyValuePair> SkipList::range(int64_t start_key, int64_t end_key) const
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);

        std::vector<KeyValuePair> results;

        // Find the node just before or at start_key
        SkipListNode *current = head.get();

        for (int level = constants::MAX_SKIP_LIST_HEIGHT - 1; level >= 0; --level)
        {
            while (current->next(level) != tail.get() && current->next(level)->get_key() < start_key)
            {
                current = current->next(level);
            }
        }

        // Move to the next node, which is >= start_key
        current = current->next(0);

        // Collect all nodes until we reach end_key or the end of the list
        while (current != tail.get() && current->get_key() < end_key)
        {
            results.emplace_back(current->get_key(), current->get_value());
            current = current->next(0);
        }

        return results;
    }

    bool SkipList::is_full() const
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);
        return current_size >= constants::BUFFER_SIZE_BYTES;
    }

    size_t SkipList::size_bytes() const
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);
        return current_size;
    }

    size_t SkipList::element_count() const
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);
        return num_elements;
    }

    void SkipList::clear()
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);

        // Create new sentinel nodes
        head = std::make_unique<SkipListNode>(constants::MAX_SKIP_LIST_HEIGHT);
        tail = std::make_unique<SkipListNode>(constants::MAX_SKIP_LIST_HEIGHT);

        // Connect head to tail at all levels
        for (int i = 0; i < constants::MAX_SKIP_LIST_HEIGHT; ++i)
        {
            head->set_next(i, tail.get());
        }

        // Reset size and count
        current_size = 0;
        num_elements = 0;
    }

    std::vector<KeyValuePair> SkipList::get_all_sorted() const
    {
        std::lock_guard<std::mutex> lock(skip_list_mutex);

        std::vector<KeyValuePair> results;
        results.reserve(num_elements);

        // Start after the head sentinel
        SkipListNode *current = head->next(0);

        // Collect all nodes until we reach the tail sentinel
        while (current != tail.get())
        {
            results.emplace_back(current->get_key(), current->get_value());
            current = current->next(0);
        }

        return results;
    }

    int SkipList::random_height()
    {
        // Probability distribution based on p = 1/4
        std::geometric_distribution<int> dist(0.75);

        // Generate height (min 1, max MAX_HEIGHT)
        return std::min(dist(rng) + 1, constants::MAX_SKIP_LIST_HEIGHT);
    }

    void SkipList::find_predecessors(int64_t key, std::vector<SkipListNode *> &predecessors) const
    {
        SkipListNode *current = head.get();

        for (int level = constants::MAX_SKIP_LIST_HEIGHT - 1; level >= 0; --level)
        {
            while (current->next(level) != tail.get() && current->next(level)->get_key() < key)
            {
                current = current->next(level);
            }
            predecessors[level] = current;
        }
    }

    size_t SkipList::estimate_pair_size(int64_t key, int64_t value, int height) const
    {
        // Key + value + next pointers + node overhead
        return sizeof(key) + sizeof(value) + sizeof(SkipListNode *) * height +
               sizeof(SkipListNode) - sizeof(std::vector<SkipListNode *>);
    }

} // namespace lsm