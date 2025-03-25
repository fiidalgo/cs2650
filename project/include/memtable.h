#ifndef LSM_MEMTABLE_H
#define LSM_MEMTABLE_H

#include "common.h"
#include "skiplist.h"
#include <string>
#include <memory>
#include <mutex>
#include <limits>

namespace lsm
{

    // MemTable class - in-memory buffer for LSM-tree
    class MemTable
    {
    private:
        using SkipListType = SkipList<Key, Value>;
        std::unique_ptr<SkipListType> skiplist_;
        size_t size_bytes_;
        size_t entry_count_;
        std::string wal_path_;
        bool immutable_;
        mutable std::mutex mutex_;

        // Estimate size of an entry in bytes
        size_t estimateEntrySize([[maybe_unused]] const Key &key, [[maybe_unused]] const Value &value) const
        {
            // Key (8 bytes) + Value (8 bytes) + deleted flag (1 byte) + overhead (8 bytes)
            return sizeof(Key) + sizeof(Value) + 1 + 8;
        }

    public:
        MemTable(const std::string &wal_path = "")
            : skiplist_(std::make_unique<SkipListType>()),
              size_bytes_(0),
              entry_count_(0),
              wal_path_(wal_path),
              immutable_(false) {}

        // Insert a key-value pair
        Status put(const Key &key, const Value &value)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (immutable_)
            {
                return Status::NOT_SUPPORTED;
            }

            // Check if key exists
            Value old_value;
            bool is_deleted;
            bool exists = skiplist_->find(key, old_value, is_deleted);

            // Update size tracking
            if (exists)
            {
                // Key exists, just update value
                skiplist_->insert(key, value, false);
            }
            else
            {
                // New key
                skiplist_->insert(key, value, false);
                size_bytes_ += estimateEntrySize(key, value);
                entry_count_++;
            }

            // Write to WAL (not implemented for basic version)

            return Status::OK;
        }

        // Get a value for a key
        Status get(const Key &key, Value &value) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            bool is_deleted;
            if (skiplist_->find(key, value, is_deleted))
            {
                if (is_deleted)
                {
                    return Status::NOT_FOUND;
                }
                return Status::OK;
            }
            return Status::NOT_FOUND;
        }

        // Delete a key (mark as deleted)
        Status remove(const Key &key)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (immutable_)
            {
                return Status::NOT_SUPPORTED;
            }

            Value value;
            bool is_deleted;
            bool exists = skiplist_->find(key, value, is_deleted);

            if (exists && !is_deleted)
            {
                skiplist_->insert(key, value, true);
                return Status::OK;
            }
            else if (!exists)
            {
                // Insert a tombstone
                skiplist_->insert(key, 0, true);
                size_bytes_ += estimateEntrySize(key, 0);
                entry_count_++;
                return Status::OK;
            }

            return Status::NOT_FOUND;
        }

        // Range query
        Status range(const Key &start_key, const Key &end_key,
                     std::vector<std::pair<Key, Value>> &results) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            results = skiplist_->range(start_key, end_key);
            return Status::OK;
        }

        // Make this MemTable immutable (for flushing)
        void makeImmutable()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            immutable_ = true;
        }

        // Check if MemTable is immutable
        bool isImmutable() const
        {
            return immutable_;
        }

        // Get current size in bytes
        size_t sizeBytes() const
        {
            return size_bytes_;
        }

        // Get number of entries
        size_t entryCount() const
        {
            return entry_count_;
        }

        // Get iterator to beginning
        typename SkipListType::Iterator begin() const
        {
            return skiplist_->begin();
        }

        // Serialize MemTable to SSTable (basic implementation)
        Status flush(const std::string &file_path) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            TrackedFile file(file_path, false);
            if (!file.isOpen())
            {
                return Status::IO_ERROR;
            }

            // Write header (format version, entry count)
            uint32_t format_version = 1;
            uint64_t count = entry_count_;

            // Find min and max keys
            Key min_key = std::numeric_limits<Key>::max();
            Key max_key = std::numeric_limits<Key>::min();

            auto it = skiplist_->begin();
            if (it.isValid())
            {
                min_key = it.key();

                // Find max key (last key in skiplist)
                auto it_end = skiplist_->begin();
                while (it_end.isValid())
                {
                    max_key = it_end.key();
                    it_end.next();
                }
            }

            file.write(&format_version, sizeof(format_version));
            file.write(&count, sizeof(count));
            file.write(&min_key, sizeof(min_key));
            file.write(&max_key, sizeof(max_key));

            // Write entries in sorted order
            it = skiplist_->begin();
            while (it.isValid())
            {
                Key key = it.key();
                Value value = it.value();
                bool is_deleted = it.isDeleted();

                file.write(&key, sizeof(key));
                file.write(&value, sizeof(value));
                file.write(&is_deleted, sizeof(is_deleted));

                it.next();
            }

            return Status::OK;
        }
    };

} // namespace lsm

#endif // LSM_MEMTABLE_H