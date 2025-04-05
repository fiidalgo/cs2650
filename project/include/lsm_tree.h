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

        // Compaction thresholds and configurations
        size_t LEVEL0_SIZE_THRESHOLD = 4; // Trigger compaction when L0 has 4 SSTables
        size_t LEVEL_SIZE_RATIO = 10;     // Size ratio between levels
        std::atomic<bool> compaction_in_progress_;
        std::atomic<uint64_t> compaction_count_; // Number of compactions performed

        bool compaction_disabled_ = false;
        size_t level0_size_threshold_ = 4; // Default threshold
        size_t level_size_ratio_ = 10;     // Default ratio between levels

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

        // Generate a unique SSTable path for a level
        std::string generateSSTablePath(size_t level)
        {
            return generateSSTableFilename(level);
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

                // Check if we need to trigger compaction
                if (levels_[0].size() >= LEVEL0_SIZE_THRESHOLD)
                {
                    triggerCompaction(0);
                }
            }

            return status;
        }

        // Background thread for flushing and compaction
        void backgroundWork()
        {
            while (running_)
            {
                // Sleep for a short time
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // See if we need to perform compaction
                if (!compaction_in_progress_ && needsCompaction())
                {
                    compaction_in_progress_ = true;

                    // First, check if L0 needs compaction
                    if (levels_.size() > 0 && levels_[0].size() >= level0_size_threshold_)
                    {
                        compactLevel0();
                    }
                    else
                    {
                        // Check which other level needs compaction
                        for (size_t i = 1; i < levels_.size(); i++)
                        {
                            size_t max_tables = level0_size_threshold_ * std::pow(level_size_ratio_, i - 1);
                            if (levels_[i].size() > max_tables)
                            {
                                compactLowerLevels(i);
                                break;
                            }
                        }
                    }

                    compaction_in_progress_ = false;
                }
            }
        }

        // Check if MemTable should be flushed
        bool shouldFlushMemTable() const
        {
            // Use smaller thresholds for testing
            const size_t TEST_MEMTABLE_SIZE = 4 * 1024; // 4KB (much smaller than default)
            const size_t TEST_MEMTABLE_ENTRIES = 1000;  // 1000 entries (instead of 1M)

            return active_memtable_->sizeBytes() >= TEST_MEMTABLE_SIZE ||
                   active_memtable_->entryCount() >= TEST_MEMTABLE_ENTRIES;
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

        // Check if compaction is needed
        bool needsCompaction() const
        {
            if (compaction_disabled_)
            {
                return false;
            }

            // Check if L0 has enough SSTables to trigger compaction
            if (levels_.size() > 0 && levels_[0].size() >= level0_size_threshold_)
            {
                return true;
            }

            // Check if any level exceeds its size limit
            for (size_t i = 1; i < levels_.size(); i++)
            {
                size_t max_tables = level0_size_threshold_ * std::pow(level_size_ratio_, i - 1);
                if (levels_[i].size() > max_tables)
                {
                    return true;
                }
            }

            return false;
        }

        // Check if a specific level needs compaction
        bool shouldCompactLevel(size_t level)
        {
            if (level >= levels_.size())
            {
                return false;
            }

            // Level 0 compaction is triggered when it has too many files
            if (level == 0)
            {
                bool should_compact = levels_[0].size() >= LEVEL0_SIZE_THRESHOLD;
                return should_compact;
            }

            // Other levels are compacted when they exceed their size threshold
            // Size threshold for level L is L0_threshold * LEVEL_SIZE_RATIO^L
            size_t max_size = LEVEL0_SIZE_THRESHOLD * std::pow(LEVEL_SIZE_RATIO, level);
            bool should_compact = levels_[level].size() > max_size;
            return should_compact;
        }

        // Trigger compaction for a level
        void triggerCompaction(size_t level)
        {
            std::cout << "DEBUG: triggerCompaction called for level " << level << std::endl;

            // Check if compaction is already in progress
            if (compaction_in_progress_)
            {
                std::cout << "DEBUG: Compaction already in progress, cannot trigger for level " << level << std::endl;
                return;
            }

            // Check if this level actually needs compaction
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!shouldCompactLevel(level))
                {
                    std::cout << "DEBUG: Level " << level << " doesn't need compaction according to shouldCompactLevel" << std::endl;
                    return;
                }
            }

            // Set flag and start compaction
            compaction_in_progress_ = true;
            std::cout << "DEBUG: Set compaction_in_progress_ to true for level " << level << std::endl;

            // Schedule compaction in background thread
            flush_cv_.notify_one();
        }

        // Compact a level using leveling compaction strategy
        void compactLevel(size_t level)
        {
            compaction_in_progress_ = true;
            std::cout << "DEBUG: Starting compaction of level " << level << " with " << (level < levels_.size() ? levels_[level].size() : 0) << " files" << std::endl;

            try
            {
                std::vector<std::unique_ptr<SSTable>> tables_to_compact;
                std::vector<std::unique_ptr<SSTable>> next_level_tables;

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    // Ensure we have enough levels
                    while (levels_.size() <= level + 1)
                    {
                        levels_.push_back({});
                        std::cout << "DEBUG: Created new level " << levels_.size() - 1 << std::endl;
                    }

                    // Collect SSTables from the current level
                    std::cout << "DEBUG: Collecting " << levels_[level].size() << " tables from level " << level << std::endl;
                    while (!levels_[level].empty())
                    {
                        tables_to_compact.push_back(std::move(levels_[level].back()));
                        levels_[level].pop_back();
                    }

                    // If compacting level 0 to level 1 or higher, move all tables from level 0
                    // For other levels, perform leveling compaction (merge with next level)
                    if (level < levels_.size() - 1)
                    {
                        // Collect SSTables from the next level
                        std::cout << "DEBUG: Collecting " << levels_[level + 1].size() << " tables from level " << (level + 1) << std::endl;
                        while (!levels_[level + 1].empty())
                        {
                            next_level_tables.push_back(std::move(levels_[level + 1].back()));
                            levels_[level + 1].pop_back();
                        }
                    }
                }

                // If no tables to compact, we're done
                if (tables_to_compact.empty())
                {
                    std::cout << "DEBUG: No tables to compact for level " << level << std::endl;
                    compaction_in_progress_ = false;
                    return;
                }

                // Merge SSTables to create new ones for the next level
                std::cout << "DEBUG: Performing merge from level " << level << " to " << (level + 1) << " with " << tables_to_compact.size() << " source tables and " << next_level_tables.size() << " target tables" << std::endl;

                std::vector<std::unique_ptr<SSTable>> merged_tables = performMerge(
                    std::move(tables_to_compact),
                    std::move(next_level_tables),
                    level + 1);

                std::cout << "DEBUG: Merge created " << merged_tables.size() << " new tables for level " << (level + 1) << std::endl;

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    // Add merged tables to the next level
                    for (auto &table : merged_tables)
                    {
                        levels_[level + 1].push_back(std::move(table));
                    }
                }

                // Increment compaction count
                compaction_count_++;
                std::cout << "DEBUG: Completed compaction of level " << level << ", new compaction count: " << compaction_count_ << std::endl;

                // Check if the next level now needs compaction
                if (shouldCompactLevel(level + 1))
                {
                    std::cout << "DEBUG: Level " << (level + 1) << " now needs compaction" << std::endl;
                    triggerCompaction(level + 1);
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "ERROR during compaction: " << e.what() << std::endl;
            }

            compaction_in_progress_ = false;
            std::cout << "DEBUG: Compaction of level " << level << " finished, compaction_in_progress_ set to false" << std::endl;
        }

        // Merge multiple SSTables to create new ones for the target level
        std::vector<std::unique_ptr<SSTable>> performMerge(
            std::vector<std::unique_ptr<SSTable>> source_tables,
            std::vector<std::unique_ptr<SSTable>> target_tables,
            size_t target_level)
        {
            std::cout << "DEBUG: performMerge starting with " << source_tables.size() << " source tables and " << target_tables.size() << " target tables" << std::endl;

            std::vector<std::unique_ptr<SSTable>> result_tables;

            // Create a temporary MemTable to hold merged data
            MemTable merged_data;
            std::cout << "DEBUG: Created temporary MemTable for merged data" << std::endl;

            // Process all source tables
            for (auto &table : source_tables)
            {
                std::cout << "DEBUG: Processing source table with " << table->entryCount() << " entries" << std::endl;
                std::vector<std::pair<Key, Value>> entries;
                table->range(std::numeric_limits<Key>::min(), std::numeric_limits<Key>::max(), entries);

                std::cout << "DEBUG: Retrieved " << entries.size() << " entries from source table" << std::endl;
                for (const auto &entry : entries)
                {
                    merged_data.put(entry.first, entry.second);
                }
            }

            // Process all target tables
            for (auto &table : target_tables)
            {
                std::cout << "DEBUG: Processing target table with " << table->entryCount() << " entries" << std::endl;
                std::vector<std::pair<Key, Value>> entries;
                table->range(std::numeric_limits<Key>::min(), std::numeric_limits<Key>::max(), entries);

                std::cout << "DEBUG: Retrieved " << entries.size() << " entries from target table" << std::endl;
                for (const auto &entry : entries)
                {
                    merged_data.put(entry.first, entry.second);
                }
            }

            // If merged data is empty, return empty result
            if (merged_data.entryCount() == 0)
            {
                std::cout << "DEBUG: No data to merge, returning empty result" << std::endl;
                return result_tables;
            }

            std::cout << "DEBUG: Merged data has " << merged_data.entryCount() << " entries" << std::endl;

            // Create a new SSTable with the merged data
            std::string merged_file_path = generateSSTableFilename(target_level);
            std::cout << "DEBUG: Creating new SSTable at " << merged_file_path << std::endl;
            Status status = merged_data.flush(merged_file_path);

            if (status == Status::OK)
            {
                std::cout << "DEBUG: Successfully created merged SSTable" << std::endl;
                result_tables.push_back(std::make_unique<SSTable>(merged_file_path));
            }
            else
            {
                std::cerr << "ERROR creating merged SSTable: " << statusToString(status) << std::endl;
            }

            std::cout << "DEBUG: Returning " << result_tables.size() << " merged tables" << std::endl;
            return result_tables;
        }

        // Load existing SSTables from disk
        void loadExistingSSTables()
        {
            if (!fileExists(data_dir_))
            {
                createDirectory(data_dir_);
                return;
            }

            std::regex sstable_regex("L(\\d+)-\\d+-\\d+\\.sst");

            for (const auto &entry : directoryIterator(data_dir_))
            {
                if (!entry.is_regular_file())
                    continue;

                std::string file_path = entry.path();
                std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);

                std::smatch matches;
                if (std::regex_match(filename, matches, sstable_regex))
                {
                    int level = std::stoi(matches[1]);

                    // Ensure we have enough levels
                    if (level >= static_cast<int>(levels_.size()))
                    {
                        levels_.resize(level + 1);
                    }

                    // Create and add SSTable
                    auto sstable = std::make_unique<SSTable>(file_path);
                    levels_[level].push_back(std::move(sstable));
                }
            }
        }

    public:
        LSMTree(const std::string &data_dir)
            : data_dir_(data_dir),
              active_memtable_(std::make_unique<MemTable>()),
              running_(true),
              compaction_in_progress_(false),
              compaction_count_(0)
        {
            // Ensure data directory exists
            if (!fileExists(data_dir_))
            {
                createDirectory(data_dir_);
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

        // Set compaction disabled or enabled
        void setCompactionDisabled(bool disabled)
        {
            compaction_disabled_ = disabled;
        }

        bool isCompactionDisabled() const
        {
            return compaction_disabled_;
        }

        // Set the threshold for L0 compaction
        void setLevel0Threshold(size_t threshold)
        {
            level0_size_threshold_ = threshold;
        }

        size_t getLevel0Threshold() const
        {
            return level0_size_threshold_;
        }

        // Set the size ratio between levels
        void setLevelSizeRatio(size_t ratio)
        {
            level_size_ratio_ = ratio;
        }

        size_t getLevelSizeRatio() const
        {
            return level_size_ratio_;
        }

        size_t getCompactionCount() const
        {
            return compaction_count_;
        }

        // Get the count of SSTables in all levels
        size_t getTotalSSTableCount() const
        {
            size_t count = 0;
            for (const auto &level : levels_)
            {
                count += level.size();
            }
            return count;
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

            // Helper to merge results from different sources
            auto merge_results = [&results](const std::vector<std::pair<Key, Value>> &level_results)
            {
                // Simple merging approach: collect all keys and remove duplicates later
                results.insert(results.end(), level_results.begin(), level_results.end());
            };

            // Get results from active memtable
            std::vector<std::pair<Key, Value>> level_results;
            active_memtable_->range(start_key, end_key, level_results);
            merge_results(level_results);

            // Get results from immutable memtables
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &memtable : immutable_memtables_)
                {
                    level_results.clear();
                    memtable->range(start_key, end_key, level_results);
                    merge_results(level_results);
                }
            }

            // Get results from SSTables
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (size_t i = 0; i < levels_.size(); i++)
                {
                    for (auto it = levels_[i].rbegin(); it != levels_[i].rend(); ++it)
                    {
                        level_results.clear();
                        (*it)->range(start_key, end_key, level_results);
                        merge_results(level_results);
                    }
                }
            }

            // Remove duplicates (keeping the newest version of each key)
            std::sort(results.begin(), results.end(), [](const auto &a, const auto &b)
                      { return a.first < b.first; });

            auto new_end = std::unique(results.begin(), results.end(),
                                       [](const auto &a, const auto &b)
                                       { return a.first == b.first; });

            results.erase(new_end, results.end());

            return Status::OK;
        }

        // Flush all memtables to disk
        Status flushAllMemTables()
        {
            // Flush active memtable
            if (active_memtable_->entryCount() > 0)
            {
                // Instead of copying, create a new MemTable and swap with the active one
                std::string file_path = generateSSTableFilename(0);
                Status status = active_memtable_->flush(file_path);

                if (status == Status::OK)
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    levels_[0].push_back(std::make_unique<SSTable>(file_path));

                    // Create a new active memtable
                    active_memtable_ = std::make_unique<MemTable>();
                }
                else
                {
                    return status;
                }
            }

            // Flush all immutable memtables
            std::vector<std::unique_ptr<MemTable>> tables;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                tables.swap(immutable_memtables_);
            }

            for (auto &memtable : tables)
            {
                std::string file_path = generateSSTableFilename(0);
                Status status = memtable->flush(file_path);

                if (status == Status::OK)
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    levels_[0].push_back(std::make_unique<SSTable>(file_path));
                }
                else
                {
                    return status;
                }
            }

            return Status::OK;
        }

        // Get database stats
        void getStats(std::map<std::string, std::string> &stats)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Count total entries
            size_t total_entries = active_memtable_->entryCount();
            for (const auto &memtable : immutable_memtables_)
            {
                total_entries += memtable->entryCount();
            }

            size_t total_sstables = 0;
            for (size_t i = 0; i < levels_.size(); i++)
            {
                std::string level_key = "level_" + std::to_string(i) + "_files";
                stats[level_key] = std::to_string(levels_[i].size());
                total_sstables += levels_[i].size();

                size_t level_entries = 0;
                for (const auto &sstable : levels_[i])
                {
                    level_entries += sstable->entryCount();
                }
                total_entries += level_entries;

                std::string level_entries_key = "level_" + std::to_string(i) + "_entries";
                stats[level_entries_key] = std::to_string(level_entries);
            }

            stats["total_entries"] = std::to_string(total_entries);
            stats["total_sstables"] = std::to_string(total_sstables);
            stats["active_memtable_entries"] = std::to_string(active_memtable_->entryCount());
            stats["immutable_memtables"] = std::to_string(immutable_memtables_.size());
            stats["compactions_performed"] = std::to_string(compaction_count_);
        }

        // Manual compaction command
        Status compact()
        {
            // First, flush the active memtable if it has entries
            if (active_memtable_->entryCount() > 0)
            {
                Status status = flushAllMemTables();
                if (status != Status::OK)
                {
                    return status;
                }
                // Wait a moment for flush to complete
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Check if compaction is already in progress
            if (compaction_in_progress_)
            {
                return Status::NOT_SUPPORTED;
            }

            // Find first level that needs compaction
            size_t level_to_compact = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (size_t i = 0; i < levels_.size(); i++)
                {
                    if (shouldCompactLevel(i))
                    {
                        level_to_compact = i;
                        break;
                    }
                }
            }

            // Trigger compaction
            triggerCompaction(level_to_compact);

            return Status::OK;
        }

        // Add an SSTable directly to level 0 (for testing)
        void addSSTableToLevel0(const std::string &file_path)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            levels_[0].push_back(std::make_unique<SSTable>(file_path));
        }

        // Get active memtable (for testing)
        MemTable *getActiveMemTable() const
        {
            return active_memtable_.get();
        }

        void compactLevel0()
        {
            if (levels_.size() == 0 || levels_[0].size() < level0_size_threshold_)
            {
                return; // Nothing to compact
            }

            // If level 1 doesn't exist, create it
            if (levels_.size() == 1)
            {
                levels_.resize(2);
            }

            // Merge all L0 SSTables into a single new L1 SSTable
            std::vector<KeyValue> merged_data;

            // Collect all data from L0 SSTables
            for (const auto &sstable : levels_[0])
            {
                std::vector<KeyValue> data = sstable->readAll();
                merged_data.insert(merged_data.end(), data.begin(), data.end());
            }

            // Sort and deduplicate the merged data
            std::sort(merged_data.begin(), merged_data.end());
            auto last = std::unique(merged_data.begin(), merged_data.end(),
                                    [](const KeyValue &a, const KeyValue &b)
                                    { return a.key == b.key; });
            merged_data.erase(last, merged_data.end());

            // Write the merged data to a new L1 SSTable
            if (!merged_data.empty())
            {
                std::string new_sstable_path = generateSSTablePath(1);
                auto new_sstable = std::make_unique<SSTable>(new_sstable_path, merged_data);
                levels_[1].push_back(std::move(new_sstable));
            }

            // Clear L0
            levels_[0].clear();

            // Increment compaction count
            compaction_count_++;

            // Check if L1 needs further compaction
            compactLowerLevels(1);
        }

        void compactLowerLevels(size_t level)
        {
            // If this level doesn't have too many SSTables, we're done
            size_t max_tables = level0_size_threshold_ * std::pow(level_size_ratio_, level - 1);
            if (level >= levels_.size() || levels_[level].size() <= max_tables)
            {
                return;
            }

            // Ensure the next level exists
            if (level + 1 >= levels_.size())
            {
                levels_.resize(level + 2);
            }

            // Merge all this level's SSTables into a single new SSTable for the next level
            std::vector<KeyValue> merged_data;

            // Collect all data from this level's SSTables
            for (const auto &sstable : levels_[level])
            {
                std::vector<KeyValue> data = sstable->readAll();
                merged_data.insert(merged_data.end(), data.begin(), data.end());
            }

            // Sort and deduplicate the merged data
            std::sort(merged_data.begin(), merged_data.end());
            auto last = std::unique(merged_data.begin(), merged_data.end(),
                                    [](const KeyValue &a, const KeyValue &b)
                                    { return a.key == b.key; });
            merged_data.erase(last, merged_data.end());

            // Write the merged data to a new SSTable in the next level
            if (!merged_data.empty())
            {
                std::string new_sstable_path = generateSSTablePath(level + 1);
                auto new_sstable = std::make_unique<SSTable>(new_sstable_path, merged_data);
                levels_[level + 1].push_back(std::move(new_sstable));
            }

            // Clear this level
            levels_[level].clear();

            // Increment compaction count
            compaction_count_++;

            // Check if the next level needs further compaction
            compactLowerLevels(level + 1);
        }
    };

} // namespace lsm

#endif // LSM_TREE_H