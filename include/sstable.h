#ifndef LSM_SSTABLE_H
#define LSM_SSTABLE_H

#include "common.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace lsm
{

    // SSTable class - on-disk sorted string table
    class SSTable
    {
    private:
        std::string file_path_;
        uint64_t entry_count_;
        Key min_key_;
        Key max_key_;

        // Basic SSTable format:
        // - Header: format_version(4), entry_count(8), min_key(8), max_key(8)
        // - Data: [key(8), value(8), is_deleted(1)] * entry_count

        static constexpr size_t HEADER_SIZE = 4 + 8 + 8 + 8;
        static constexpr size_t ENTRY_SIZE = sizeof(Key) + sizeof(Value) + 1;

    public:
        SSTable(const std::string &file_path)
            : file_path_(file_path), entry_count_(0), min_key_(0), max_key_(0)
        {
            // Read header if file exists
            if (fileExists(file_path))
            {
                TrackedFile file(file_path, true);
                if (file.isOpen())
                {
                    uint32_t format_version;
                    file.read(&format_version, sizeof(format_version));
                    file.read(&entry_count_, sizeof(entry_count_));
                    file.read(&min_key_, sizeof(min_key_));
                    file.read(&max_key_, sizeof(max_key_));
                }
            }
        }

        // Check if key might be in this SSTable
        bool mayContainKey(const Key &key) const
        {
            return key >= min_key_ && key <= max_key_;
        }

        // Get a value for a key
        Status get(const Key &key, Value &value) const
        {
            if (!mayContainKey(key))
            {
                return Status::NOT_FOUND;
            }

            TrackedFile file(file_path_, true);
            if (!file.isOpen())
            {
                return Status::IO_ERROR;
            }

            // Skip header
            file.seek(HEADER_SIZE);

            // Linear search through entries (unoptimized version)
            for (uint64_t i = 0; i < entry_count_; i++)
            {
                Key current_key;
                Value current_value;
                bool is_deleted;

                file.read(&current_key, sizeof(current_key));
                file.read(&current_value, sizeof(current_value));
                file.read(&is_deleted, sizeof(is_deleted));

                if (current_key == key)
                {
                    if (is_deleted)
                    {
                        return Status::NOT_FOUND;
                    }
                    value = current_value;
                    return Status::OK;
                }

                // Early termination if we've passed the key
                if (current_key > key)
                {
                    break;
                }
            }

            return Status::NOT_FOUND;
        }

        // Range query
        Status range(const Key &start_key, const Key &end_key,
                     std::vector<std::pair<Key, Value>> &results) const
        {

            // Check if range overlaps with SSTable key range
            if (end_key < min_key_ || start_key > max_key_)
            {
                return Status::OK; // No overlap
            }

            TrackedFile file(file_path_, true);
            if (!file.isOpen())
            {
                return Status::IO_ERROR;
            }

            // Skip header
            file.seek(HEADER_SIZE);

            // Scan through entries
            for (uint64_t i = 0; i < entry_count_; i++)
            {
                Key current_key;
                Value current_value;
                bool is_deleted;

                file.read(&current_key, sizeof(current_key));
                file.read(&current_value, sizeof(current_value));
                file.read(&is_deleted, sizeof(is_deleted));

                // Skip entries before start_key
                if (current_key < start_key)
                {
                    continue;
                }

                // Stop if we've passed end_key
                if (current_key > end_key)
                {
                    break;
                }

                // Add to results if not deleted
                if (!is_deleted)
                {
                    results.emplace_back(current_key, current_value);
                }
            }

            return Status::OK;
        }

        // Create a new SSTable from a MemTable
        static Status createFromMemTable(const MemTable &memtable, const std::string &file_path)
        {
            TrackedFile file(file_path, false);
            if (!file.isOpen())
            {
                return Status::IO_ERROR;
            }

            // Reserve space for header
            uint32_t format_version = 1;
            uint64_t entry_count = memtable.entryCount();
            Key min_key = std::numeric_limits<Key>::max();
            Key max_key = std::numeric_limits<Key>::min();

            // Write placeholder header (will update later)
            file.write(&format_version, sizeof(format_version));
            file.write(&entry_count, sizeof(entry_count));
            file.write(&min_key, sizeof(min_key));
            file.write(&max_key, sizeof(max_key));

            // Write entries
            auto it = memtable.begin();
            bool first_entry = true;

            while (it.isValid())
            {
                Key key = it.key();
                Value value = it.value();
                bool is_deleted = it.isDeleted();

                // Update min/max keys
                if (first_entry || key < min_key)
                {
                    min_key = key;
                }
                if (first_entry || key > max_key)
                {
                    max_key = key;
                }
                first_entry = false;

                // Write entry
                file.write(&key, sizeof(key));
                file.write(&value, sizeof(value));
                file.write(&is_deleted, sizeof(is_deleted));

                it.next();
            }

            // Update header with correct min/max keys
            file.seek(4 + 8); // Skip format_version and entry_count
            file.write(&min_key, sizeof(min_key));
            file.write(&max_key, sizeof(max_key));

            return Status::OK;
        }

        // Get file path
        const std::string &filePath() const
        {
            return file_path_;
        }

        // Get entry count
        uint64_t entryCount() const
        {
            return entry_count_;
        }

        // Get min key
        Key minKey() const
        {
            return min_key_;
        }

        // Get max key
        Key maxKey() const
        {
            return max_key_;
        }
    };

} // namespace lsm

#endif // LSM_SSTABLE_H