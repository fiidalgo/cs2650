#ifndef LSM_TREE_H
#define LSM_TREE_H

#include "common.h"
#include "memtable.h"
#include "sstable.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <filesystem>
#include <regex>
#include <iostream>

namespace lsm
{

    // LSM-Tree class - main key-value store implementation
    class LSMTree
    {
    private:
        std::string data_dir_;
        std::unique_ptr<MemTable> active_memtable_;
        std::vector<std::unique_ptr<MemTable>> immutable_memtables_;
        std::vector<std::vector<std::unique_ptr<SSTable>>> levels_;

        std::mutex mutex_;
        std::condition_variable flush_cv_;
        std::atomic<bool> running_;
        std::thread background_thread_;

        // Generate a unique SSTable filename for a level
        std::string generateSSTableFilename(size_t level)
        {
            static std::atomic<uint64_t> counter(0);
            std::stringstream ss;
            ss << data_dir_ << "/L" << level << "-"
               << std::chrono::system_clock::now().time_since_epoch().count()
               << "-" << counter++ << ".sst";
            return ss.str();
        }

        // Flush an immutable MemTable to Level 0
        Status flushMemTable(MemTable *memtable)
        {
            std::string file_path = generateSSTableFilename(0);
            Status status = memtable->flush(file_path);

            if (status == Status::OK)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                levels_[0].push_back(std::make_unique<SSTable>(file_path));
            }

            return status;
        }

        // Background thread for flushing and compaction
        void backgroundWork()
        {
            while (running_)
            {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    flush_cv_.wait_for(lock, std::chrono::seconds(1), [this]
                                       { return !running_ || !immutable_memtables_.empty(); });

                    if (!running_)
                    {
                        break;
                    }

                    // Process immutable memtables
                    if (!immutable_memtables_.empty())
                    {
                        auto memtable = std::move(immutable_memtables_.front());
                        immutable_memtables_.erase(immutable_memtables_.begin());
                        lock.unlock();

                        flushMemTable(memtable.get());
                        continue;
                    }
                }

                // No work to do, sleep for a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Check if MemTable should be flushed
        bool shouldFlushMemTable() const
        {
            return active_memtable_->sizeBytes() >= DEFAULT_MEMTABLE_SIZE ||
                   active_memtable_->entryCount() >= DEFAULT_MEMTABLE_ENTRIES;
        }

        // Trigger MemTable flush
        void triggerMemTableFlush()
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Make current memtable immutable
            active_memtable_->makeImmutable();
            immutable_memtables_.push_back(std::move(active_memtable_));

            // Create a new active memtable
            active_memtable_ = std::make_unique<MemTable>();

            // Notify background thread
            flush_cv_.notify_one();
        }

        // Load existing SSTables from disk
        void loadExistingSSTables()
        {
            if (!std::filesystem::exists(data_dir_))
            {
                std::filesystem::create_directories(data_dir_);
                return;
            }

            std::regex sstable_regex("L(\\d+)-\\d+-\\d+\\.sst");

            for (const auto &entry : std::filesystem::directory_iterator(data_dir_))
            {
                if (!entry.is_regular_file())
                    continue;

                std::string filename = entry.path().filename().string();
                std::smatch match;

                if (std::regex_match(filename, match, sstable_regex))
                {
                    size_t level = std::stoul(match[1]);

                    // Ensure we have enough levels
                    while (levels_.size() <= level)
                    {
                        levels_.push_back({});
                    }

                    // Add SSTable to the appropriate level
                    levels_[level].push_back(std::make_unique<SSTable>(entry.path().string()));
                }
            }
        }

    public:
        LSMTree(const std::string &data_dir)
            : data_dir_(data_dir),
              active_memtable_(std::make_unique<MemTable>()),
              running_(true)
        {

            // Ensure data directory exists
            if (!std::filesystem::exists(data_dir_))
            {
                std::filesystem::create_directories(data_dir_);
            }

            // Initialize levels
            levels_.resize(1); // Start with just Level 0

            // Load existing SSTables
            loadExistingSSTables();

            // Start background thread
            background_thread_ = std::thread(&LSMTree::backgroundWork, this);
        }

        ~LSMTree()
        {
            // Stop background thread
            running_ = false;
            flush_cv_.notify_all();
            if (background_thread_.joinable())
            {
                background_thread_.join();
            }
        }

        // Put a key-value pair
        Status put(const Key &key, const Value &value)
        {
            // Insert into active memtable
            Status status = active_memtable_->put(key, value);

            // Check if memtable should be flushed
            if (status == Status::OK && shouldFlushMemTable())
            {
                triggerMemTableFlush();
            }

            return status;
        }

        // Get a value for a key
        Status get(const Key &key, Value &value)
        {
            // Check active memtable
            Status status = active_memtable_->get(key, value);
            if (status == Status::OK)
            {
                return status;
            }

            // Check immutable memtables
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &memtable : immutable_memtables_)
                {
                    status = memtable->get(key, value);
                    if (status == Status::OK)
                    {
                        return status;
                    }
                }
            }

            // Check SSTables in each level (newest to oldest)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (size_t i = 0; i < levels_.size(); i++)
                {
                    // Check SSTables in reverse order (newest first)
                    for (auto it = levels_[i].rbegin(); it != levels_[i].rend(); ++it)
                    {
                        if ((*it)->mayContainKey(key))
                        {
                            status = (*it)->get(key, value);
                            if (status == Status::OK)
                            {
                                return status;
                            }
                        }
                    }
                }
            }

            return Status::NOT_FOUND;
        }

        // Delete a key
        Status remove(const Key &key)
        {
            // Insert a tombstone into active memtable
            Status status = active_memtable_->remove(key);

            // Check if memtable should be flushed
            if (status == Status::OK && shouldFlushMemTable())
            {
                triggerMemTableFlush();
            }

            return status;
        }

        // Range query
        Status range(const Key &start_key, const Key &end_key,
                     std::vector<std::pair<Key, Value>> &results)
        {
            results.clear();
            std::vector<std::pair<Key, Value>> level_results;

            // Collect from active memtable
            active_memtable_->range(start_key, end_key, level_results);
            results.insert(results.end(), level_results.begin(), level_results.end());

            // Collect from immutable memtables
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &memtable : immutable_memtables_)
                {
                    level_results.clear();
                    memtable->range(start_key, end_key, level_results);
                    results.insert(results.end(), level_results.begin(), level_results.end());
                }
            }

            // Collect from SSTables
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &level : levels_)
                {
                    for (auto it = level.rbegin(); it != level.rend(); ++it)
                    {
                        level_results.clear();
                        (*it)->range(start_key, end_key, level_results);
                        results.insert(results.end(), level_results.begin(), level_results.end());
                    }
                }
            }

            // Sort and remove duplicates (keeping newest version of each key)
            std::sort(results.begin(), results.end());
            auto last = std::unique(results.begin(), results.end(),
                                    [](const auto &a, const auto &b)
                                    {
                                        return a.first == b.first;
                                    });
            results.erase(last, results.end());

            return Status::OK;
        }

        // Flush all memtables (for testing)
        Status flushAllMemTables()
        {
            // Flush active memtable
            if (active_memtable_->entryCount() > 0)
            {
                triggerMemTableFlush();
            }

            // Wait for all flushes to complete
            while (true)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (immutable_memtables_.empty())
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            return Status::OK;
        }

        // Get statistics
        void getStats(std::map<std::string, std::string> &stats)
        {
            stats["active_memtable_size"] = std::to_string(active_memtable_->sizeBytes());
            stats["active_memtable_entries"] = std::to_string(active_memtable_->entryCount());

            size_t immutable_count = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                immutable_count = immutable_memtables_.size();

                stats["level_count"] = std::to_string(levels_.size());

                for (size_t i = 0; i < levels_.size(); i++)
                {
                    stats["level_" + std::to_string(i) + "_files"] = std::to_string(levels_[i].size());
                }
            }

            stats["immutable_memtables"] = std::to_string(immutable_count);
        }

        // Get the active MemTable (for testing)
        MemTable *getActiveMemTable()
        {
            return active_memtable_.get();
        }

        // Add an SSTable to Level 0 (for testing)
        void addSSTableToLevel0(const std::string &file_path)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            levels_[0].push_back(std::make_unique<SSTable>(file_path));
        }
    };

} // namespace lsm

#endif // LSM_TREE_H