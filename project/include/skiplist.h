#ifndef LSM_SKIPLIST_H
#define LSM_SKIPLIST_H

#include "common.h"
#include <random>
#include <limits>

namespace lsm
{

    // SkipList implementation for MemTable
    template <typename K, typename V>
    class SkipList
    {
    private:
        static constexpr int MAX_LEVEL = 12; // Maximum height of skip list
        static constexpr float P = 0.5f;     // Probability for level promotion

        struct Node
        {
            K key;
            V value;
            bool is_deleted;
            std::vector<Node *> forward;

            Node(K k, V v, int level, bool deleted = false)
                : key(k), value(v), is_deleted(deleted), forward(level + 1, nullptr) {}
        };

        Node *head_;
        Node *tail_;
        int current_level_;
        size_t size_;
        std::mt19937 gen_;
        std::uniform_real_distribution<float> dis_;

        // Generate random level for a new node
        int randomLevel()
        {
            int level = 0;
            while (dis_(gen_) < P && level < MAX_LEVEL - 1)
            {
                level++;
            }
            return level;
        }

    public:
        SkipList()
            : current_level_(0), size_(0),
              gen_(std::random_device{}()), dis_(0.0f, 1.0f)
        {
            // Create head and tail sentinel nodes
            K min_key = std::numeric_limits<K>::min();
            K max_key = std::numeric_limits<K>::max();
            head_ = new Node(min_key, V{}, MAX_LEVEL);
            tail_ = new Node(max_key, V{}, MAX_LEVEL);

            // Link all levels of head to tail
            for (int i = 0; i <= MAX_LEVEL; i++)
            {
                head_->forward[i] = tail_;
            }
        }

        ~SkipList()
        {
            Node *current = head_;
            while (current != nullptr)
            {
                Node *next = current->forward[0];
                delete current;
                current = next;
            }
        }

        // Insert a key-value pair
        void insert(const K &key, const V &value, bool is_deleted = false)
        {
            std::vector<Node *> update(MAX_LEVEL + 1, nullptr);
            Node *current = head_;

            // Find position for insertion
            for (int i = current_level_; i >= 0; i--)
            {
                while (current->forward[i] != tail_ && current->forward[i]->key < key)
                {
                    current = current->forward[i];
                }
                update[i] = current;
            }
            current = current->forward[0];

            // Update existing key or insert new node
            if (current != tail_ && current->key == key)
            {
                current->value = value;
                current->is_deleted = is_deleted;
            }
            else
            {
                int new_level = randomLevel();
                if (new_level > current_level_)
                {
                    for (int i = current_level_ + 1; i <= new_level; i++)
                    {
                        update[i] = head_;
                    }
                    current_level_ = new_level;
                }

                Node *new_node = new Node(key, value, new_level, is_deleted);
                for (int i = 0; i <= new_level; i++)
                {
                    new_node->forward[i] = update[i]->forward[i];
                    update[i]->forward[i] = new_node;
                }
                size_++;
            }
        }

        // Find a key
        bool find(const K &key, V &value, bool &is_deleted) const
        {
            Node *current = head_;

            // Search from top level to bottom
            for (int i = current_level_; i >= 0; i--)
            {
                while (current->forward[i] != tail_ && current->forward[i]->key < key)
                {
                    current = current->forward[i];
                }
            }
            current = current->forward[0];

            // Check if key exists
            if (current != tail_ && current->key == key)
            {
                value = current->value;
                is_deleted = current->is_deleted;
                return true;
            }
            return false;
        }

        // Range query: return all key-value pairs in [start_key, end_key]
        std::vector<std::pair<K, V>> range(const K &start_key, const K &end_key) const
        {
            std::vector<std::pair<K, V>> result;
            Node *current = head_;

            // Find start position
            for (int i = current_level_; i >= 0; i--)
            {
                while (current->forward[i] != tail_ && current->forward[i]->key < start_key)
                {
                    current = current->forward[i];
                }
            }
            current = current->forward[0];

            // Collect all keys in range
            while (current != tail_ && current->key <= end_key)
            {
                if (!current->is_deleted)
                {
                    result.emplace_back(current->key, current->value);
                }
                current = current->forward[0];
            }
            return result;
        }

        // Delete a key (mark as deleted)
        bool remove(const K &key)
        {
            std::vector<Node *> update(MAX_LEVEL + 1, nullptr);
            Node *current = head_;

            // Find position
            for (int i = current_level_; i >= 0; i--)
            {
                while (current->forward[i] != tail_ && current->forward[i]->key < key)
                {
                    current = current->forward[i];
                }
                update[i] = current;
            }
            current = current->forward[0];

            // Mark as deleted if found
            if (current != tail_ && current->key == key)
            {
                current->is_deleted = true;
                return true;
            }
            return false;
        }

        // Get size
        size_t size() const
        {
            return size_;
        }

        // Iterator for traversing the skip list
        class Iterator
        {
        private:
            const SkipList *list_;
            Node *current_;

        public:
            Iterator(const SkipList *list, Node *start)
                : list_(list), current_(start) {}

            bool isValid() const
            {
                return current_ != list_->tail_;
            }

            void next()
            {
                if (isValid())
                {
                    current_ = current_->forward[0];
                }
            }

            K key() const
            {
                return current_->key;
            }

            V value() const
            {
                return current_->value;
            }

            bool isDeleted() const
            {
                return current_->is_deleted;
            }
        };

        // Get iterator to beginning
        Iterator begin() const
        {
            return Iterator(this, head_->forward[0]);
        }
    };

} // namespace lsm

#endif // LSM_SKIPLIST_H