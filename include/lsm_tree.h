#ifndef LSM_TREE_H
#define LSM_TREE_H

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <optional>
#include "constants.h"

// Forward declarations
namespace lsm
{
    class SkipList;
    class BloomFilter;
    class Run;
    class FencePointers;
}

namespace lsm
{

    // Enumeration of compaction strategies
    enum class CompactionStrategy
    {
        TIERING,       // Multiple runs per level, only compact when threshold reached
        LAZY_LEVELING, // Multiple runs allowed but compact in place
        LEVELING       // Single run per level
    };

    // Represents a key-value pair
    struct KeyValuePair
    {
        int64_t key;
        int64_t value;

        KeyValuePair(int64_t k, int64_t v) : key(k), value(v) {}

        // Compare operators for sorting
        bool operator<(const KeyValuePair &other) const
        {
            return key < other.key;
        }

        bool operator==(const KeyValuePair &other) const
        {
            return key == other.key;
        }
    };

    // Represents a level in the LSM-tree
    class Level
    {
    public:
        Level(int level_number, CompactionStrategy strategy);
        ~Level();

        // Add a run to this level
        void add_run(std::unique_ptr<Run> run);

        // Check if compaction is needed
        bool needs_compaction() const;

        // Get all runs in this level
        const std::vector<std::unique_ptr<Run>> &get_runs() const;

        // Get the level number
        int get_level_number() const;

        // Get the compaction strategy
        CompactionStrategy get_strategy() const;

        // Get number of runs
        size_t run_count() const;

        // Clear all runs (after compaction)
        void clear_runs();

    private:
        int level_number;
        CompactionStrategy strategy;
        std::vector<std::unique_ptr<Run>> runs;
        mutable std::mutex level_mutex;
    };

    // Main LSM-tree class
    class LSMTree
    {
    public:
        LSMTree();
        ~LSMTree();

        // Deleted copy/move constructors and assignment operators
        LSMTree(const LSMTree &) = delete;
        LSMTree &operator=(const LSMTree &) = delete;
        LSMTree(LSMTree &&) = delete;
        LSMTree &operator=(LSMTree &&) = delete;

        // Primary operations
        void put(int64_t key, int64_t value);
        std::optional<int64_t> get(int64_t key);
        std::vector<KeyValuePair> range(int64_t start_key, int64_t end_key);
        bool remove(int64_t key);

        // Batch operations
        void load_file(const std::string &filepath);

        // Management operations
        void compact();
        void rebuild_filters();
        void print_stats(std::ostream &out) const;

        // Get the total number of key-value pairs (logical count)
        size_t size() const;

        // Get string representation of compaction strategy
        std::string get_strategy_name(CompactionStrategy strategy) const;

    private:
        // In-memory buffer (skip list)
        std::unique_ptr<SkipList> buffer;

        // Levels of the LSM-tree
        std::vector<std::unique_ptr<Level>> levels;

        // Current max level (for FPR calculations)
        std::atomic<int> max_level;

        // For synchronization
        mutable std::mutex tree_mutex;

        // Internal methods
        void flush_buffer();
        void perform_compaction(int level);
        CompactionStrategy get_strategy_for_level(int level) const;
        double calculate_fpr_for_level(int level) const;

        // Get appropriate level for a new run based on size
        int get_target_level_for_size(size_t size_bytes) const;

        // Check if we need more levels
        void check_and_extend_levels();

        // Load state from disk at startup
        void load_state_from_disk();

        // Internal logging
        void log_debug(const std::string &message) const;
    };

} // namespace lsm

#endif // LSM_TREE_H