#include "../include/lsm_tree.h"
#include "../include/skip_list.h"
#include "../include/run.h"
#include "../include/bloom_filter.h"
#include "../include/fence_pointers.h"
#include "../include/constants.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <numeric>
#include <map>

namespace fs = std::filesystem;

namespace lsm
{

    // Level implementation

    Level::Level(int level_number, CompactionStrategy strategy)
        : level_number(level_number), strategy(strategy)
    {
    }

    Level::~Level()
    {
    }

    void Level::add_run(std::unique_ptr<Run> run)
    {
        std::lock_guard<std::mutex> lock(level_mutex);
        runs.push_back(std::move(run));
    }

    bool Level::needs_compaction() const
    {
        std::lock_guard<std::mutex> lock(level_mutex);

        switch (strategy)
        {
        case CompactionStrategy::TIERING:
            return runs.size() >= constants::TIERING_THRESHOLD;

        case CompactionStrategy::LAZY_LEVELING:
            return runs.size() >= constants::LAZY_LEVELING_THRESHOLD;

        case CompactionStrategy::LEVELING:
            return runs.size() > 1;

        default:
            return false;
        }
    }

    const std::vector<std::unique_ptr<Run>> &Level::get_runs() const
    {
        return runs;
    }

    int Level::get_level_number() const
    {
        return level_number;
    }

    CompactionStrategy Level::get_strategy() const
    {
        return strategy;
    }

    size_t Level::run_count() const
    {
        std::lock_guard<std::mutex> lock(level_mutex);
        return runs.size();
    }

    void Level::clear_runs()
    {
        std::lock_guard<std::mutex> lock(level_mutex);

        // First delete all physical files associated with these runs
        for (auto &run : runs)
        {
            run->delete_files_from_disk();
        }

        // Then clear the runs from memory
        runs.clear();
    }

    // LSMTree implementation

    LSMTree::LSMTree() : max_level(constants::INITIAL_MAX_LEVEL)
    {
        // Create data directory if it doesn't exist
        if (!fs::exists(constants::DATA_DIRECTORY))
        {
            fs::create_directory(constants::DATA_DIRECTORY);
        }

        // Initialize the buffer
        buffer = std::make_unique<SkipList>();

        // Initialize levels
        for (int i = 0; i <= max_level; ++i)
        {
            CompactionStrategy strategy;

            if (i == 1)
            {
                strategy = CompactionStrategy::TIERING;
            }
            else if (i >= 2 && i <= 4)
            {
                strategy = CompactionStrategy::LAZY_LEVELING;
            }
            else
            {
                strategy = CompactionStrategy::LEVELING;
            }

            levels.push_back(std::make_unique<Level>(i, strategy));
        }

        // Load existing state from disk if any
        load_state_from_disk();

        log_debug("LSM-Tree initialized with " + std::to_string(max_level) + " levels");
    }

    LSMTree::~LSMTree()
    {
        // Flush any remaining data in the buffer to disk
        if (buffer && buffer->element_count() > 0)
        {
            log_debug("Flushing buffer during shutdown to prevent data loss");
            flush_buffer();
        }
    }

    void LSMTree::put(int64_t key, int64_t value)
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> lock(tree_mutex);

        log_debug("PUT operation: Inserting key=" + std::to_string(key) +
                  ", value=" + std::to_string(value));

        // Insert/update in buffer
        buffer->insert(key, value);
        log_debug("PUT: Inserted into buffer. Buffer now has " +
                  std::to_string(buffer->element_count()) + " elements (" +
                  std::to_string(buffer->size_bytes()) + " bytes)");

        // Check if buffer is full and needs to be flushed
        if (buffer->is_full())
        {
            log_debug("PUT: Buffer is full (>= " +
                      std::to_string(constants::BUFFER_SIZE_BYTES.load()) + " bytes), flushing to disk");
            flush_buffer();
        }
        else
        {
            log_debug("PUT: Buffer not full yet, remaining capacity: " +
                      std::to_string(constants::BUFFER_SIZE_BYTES.load() - buffer->size_bytes()) + " bytes");
        }

        // Track write timing
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Update write metrics
        write_count++;
        total_write_time_ms.store(total_write_time_ms.load() + elapsed_ms);
    }

    std::optional<int64_t> LSMTree::get(int64_t key)
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        log_debug("GET operation: Searching for key=" + std::to_string(key));

        // First check the buffer
        auto buffer_result = buffer->get(key);
        if (buffer_result.has_value())
        {
            log_debug("GET: Found key in buffer, value=" + std::to_string(*buffer_result));

            // Track read timing for buffer hit
            auto end_time = std::chrono::high_resolution_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Update read metrics
            read_count++;
            total_read_time_ms.store(total_read_time_ms.load() + elapsed_ms);

            return buffer_result;
        }
        log_debug("GET: Key not found in buffer, checking disk levels");

        // Check each level, starting from the most recent
        for (const auto &level : levels)
        {
            int level_num = level->get_level_number();
            log_debug("GET: Checking level " + std::to_string(level_num) +
                      " (strategy: " + get_strategy_name(level->get_strategy()) +
                      ", runs: " + std::to_string(level->run_count()) + ")");

            // Check runs in reverse order (newest first)
            const auto &runs = level->get_runs();
            int run_idx = 0;
            for (auto it = runs.rbegin(); it != runs.rend(); ++it, ++run_idx)
            {
                log_debug("GET: Checking run " + std::to_string(run_idx) +
                          " in level " + std::to_string(level_num));

                // Check bloom filter first, if available
                if ((*it)->has_bloom_filter() && !(*it)->might_contain(key))
                {
                    log_debug("GET: Bloom filter indicates key is not in run " +
                              std::to_string(run_idx) + " of level " + std::to_string(level_num));
                    continue;
                }

                auto result = (*it)->get(key);
                if (result.has_value())
                {
                    log_debug("GET: Found key in run " + std::to_string(run_idx) +
                              " of level " + std::to_string(level_num) +
                              ", value=" + std::to_string(*result));

                    // Track read timing for disk hit
                    auto end_time = std::chrono::high_resolution_clock::now();
                    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

                    // Update read metrics
                    read_count++;
                    total_read_time_ms.store(total_read_time_ms.load() + elapsed_ms);

                    return result;
                }
                log_debug("GET: Key not found in run " + std::to_string(run_idx) +
                          " of level " + std::to_string(level_num));
            }
        }

        log_debug("GET: Key not found in any level");

        // Track read timing for miss
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Update read metrics (even for misses)
        read_count++;
        total_read_time_ms.store(total_read_time_ms.load() + elapsed_ms);

        // Key not found
        return std::nullopt;
    }

    std::vector<KeyValuePair> LSMTree::range(int64_t start_key, int64_t end_key)
    {
        if (start_key >= end_key)
        {
            return {};
        }

        // Results from all levels (buffer and disk)
        std::vector<KeyValuePair> results;

        // Get results from buffer
        auto buffer_results = buffer->range(start_key, end_key);
        results.insert(results.end(), buffer_results.begin(), buffer_results.end());

        // Get results from each level
        for (const auto &level : levels)
        {
            const auto &runs = level->get_runs();
            for (auto it = runs.rbegin(); it != runs.rend(); ++it)
            {
                auto run_results = (*it)->range(start_key, end_key);
                results.insert(results.end(), run_results.begin(), run_results.end());
            }
        }

        // Deduplicate results (keep only the newest value for each key)
        if (!results.empty())
        {
            // Sort by key
            std::sort(results.begin(), results.end(),
                      [](const KeyValuePair &a, const KeyValuePair &b)
                      {
                          return a.key < b.key;
                      });

            // Deduplicate
            auto last = std::unique(results.begin(), results.end(),
                                    [](const KeyValuePair &a, const KeyValuePair &b)
                                    {
                                        return a.key == b.key;
                                    });
            results.erase(last, results.end());
        }

        return results;
    }

    bool LSMTree::remove(int64_t key)
    {
        // In this implementation, removing is the same as putting with a special "tombstone" value
        // For this example, we'll use INT64_MIN as the tombstone value
        put(key, INT64_MIN);
        return true;
    }

    void LSMTree::load_file(const std::string &filepath)
    {
        std::ifstream file(filepath, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open file: " + filepath);
        }

        // Read key-value pairs in chunks to avoid loading the entire file into memory at once
        const size_t CHUNK_SIZE = 1000;
        std::vector<KeyValuePair> pairs;
        pairs.reserve(CHUNK_SIZE);

        int64_t key, value;
        size_t total_pairs = 0;

        while (file.read(reinterpret_cast<char *>(&key), sizeof(key)) &&
               file.read(reinterpret_cast<char *>(&value), sizeof(value)))
        {
            pairs.emplace_back(key, value);
            total_pairs++;

            // When we've read a chunk, insert them into the tree
            if (pairs.size() >= CHUNK_SIZE)
            {
                for (const auto &pair : pairs)
                {
                    put(pair.key, pair.value);
                }
                pairs.clear();
            }
        }

        // Insert any remaining pairs
        for (const auto &pair : pairs)
        {
            put(pair.key, pair.value);
        }

        log_debug("Loaded " + std::to_string(total_pairs) + " key-value pairs from file: " + filepath);
    }

    void LSMTree::compact()
    {
        std::lock_guard<std::mutex> lock(tree_mutex);

        // Check each level for compaction
        for (size_t i = 0; i < levels.size(); ++i)
        {
            if (levels[i]->needs_compaction())
            {
                perform_compaction(i);
            }
        }
    }

    void LSMTree::rebuild_filters()
    {
        std::lock_guard<std::mutex> lock(tree_mutex);

        for (size_t i = 0; i < levels.size(); ++i)
        {
            double fpr = calculate_fpr_for_level(i);
            log_debug("Rebuilding Bloom filters for level " + std::to_string(i) +
                      " with FPR: " + std::to_string(fpr));

            const auto &runs = levels[i]->get_runs();
            for (const auto &run : runs)
            {
                run->rebuild_bloom_filter(fpr);
            }
        }
    }

    void LSMTree::print_stats(std::ostream &out) const
    {
        size_t total_pairs = size();

        out << "Logical Pairs: " << total_pairs << "\n";

        // I/O Statistics
        out << "Read I/Os: " << read_io_count.load() << "\n";
        out << "Write I/Os: " << write_io_count.load() << "\n";

        // Count keys in each level
        out << "LVL0: " << buffer->element_count();
        for (size_t i = 1; i < levels.size(); ++i)
        {
            size_t level_count = 0;
            const auto &runs = levels[i]->get_runs();
            for (const auto &run : runs)
            {
                level_count += run->size();
            }

            out << ", LVL" << i << ": " << level_count;
        }
        out << "\n";

        // Calculate Bloom filter bits for each level
        for (size_t i = 1; i < levels.size(); ++i)
        { // Skip level 0 (buffer)
            double fpr = calculate_fpr_for_level(i);
            const auto &runs = levels[i]->get_runs();

            if (!runs.empty())
            {
                size_t avg_keys_per_run = 0;
                for (const auto &run : runs)
                {
                    avg_keys_per_run += run->size();
                }
                avg_keys_per_run /= runs.size();

                size_t bits = optimal_bits(avg_keys_per_run, fpr);
                out << "Level " << i << " Bloom filter: FPR=" << fpr
                    << ", Bits per element=" << (bits / avg_keys_per_run)
                    << ", Hash functions=" << optimal_hash_functions(bits, avg_keys_per_run) << "\n";
            }
        }

        // Print detailed key distribution
        out << "\nKey distribution:\n";

        // Limit for number of keys to display per run
        const size_t MAX_KEYS_TO_DISPLAY = 10;

        // Buffer keys (limited to MAX_KEYS_TO_DISPLAY)
        auto buffer_pairs = buffer->get_all_sorted();
        size_t buffer_display_count = 0;

        out << "Buffer (Level 0): ";
        for (const auto &pair : buffer_pairs)
        {
            if (pair.value != INT64_MIN) // Skip tombstones
            {
                out << pair.key << ":" << pair.value << " ";
                buffer_display_count++;

                if (buffer_display_count >= MAX_KEYS_TO_DISPLAY)
                {
                    out << "... (" << (buffer_pairs.size() - buffer_display_count) << " more)";
                    break;
                }
            }
        }
        out << "\n";

        // Keys in disk levels
        for (size_t i = 1; i < levels.size(); ++i)
        {
            const auto &runs = levels[i]->get_runs();
            if (!runs.empty())
            {
                out << "\nLevel " << i << " keys:\n";
                for (size_t j = 0; j < runs.size(); ++j)
                {
                    out << "Run " << j << " (" << runs[j]->size() << " keys): ";

                    // Get sample keys (max MAX_KEYS_TO_DISPLAY)
                    auto pairs = runs[j]->get_sample_pairs(MAX_KEYS_TO_DISPLAY);
                    size_t displayed = 0;

                    for (const auto &pair : pairs)
                    {
                        if (pair.value != INT64_MIN) // Skip tombstones
                        {
                            out << pair.key << ":" << pair.value << " ";
                            displayed++;

                            // Extra safety check to ensure we don't display too many
                            if (displayed >= MAX_KEYS_TO_DISPLAY)
                            {
                                break;
                            }
                        }
                    }

                    // Show how many more keys exist
                    size_t total_keys = runs[j]->size();
                    if (total_keys > displayed)
                    {
                        out << "... (" << (total_keys - displayed) << " more)";
                    }

                    out << "\n";
                }
            }
        }
    }

    size_t LSMTree::size() const
    {
        // Count logical key-value pairs (excluding deleted/tombstones)
        size_t count = buffer->element_count();

        for (const auto &level : levels)
        {
            const auto &runs = level->get_runs();
            for (const auto &run : runs)
            {
                count += run->size();
            }
        }

        return count;
    }

    void LSMTree::flush_buffer()
    {
        if (buffer->element_count() == 0)
        {
            log_debug("Flush called on empty buffer, nothing to do");
            return;
        }

        log_debug("Flushing buffer with " + std::to_string(buffer->element_count()) +
                  " elements (" + std::to_string(buffer->size_bytes()) + " bytes)");

        // Get all pairs from the buffer
        auto pairs = buffer->get_all_sorted();

        // Create a new run in level 1
        int level = 1;
        size_t run_id = levels[level]->run_count();
        double fpr = calculate_fpr_for_level(level);

        auto run = std::make_unique<Run>(pairs, level, run_id, fpr);
        levels[level]->add_run(std::move(run));

        // Clear the buffer
        buffer->clear();

        // Check if level 1 needs compaction after the flush
        if (constants::COMPACTION_ENABLED.load() && levels[level]->needs_compaction())
        {
            log_debug("Level " + std::to_string(level) + " needs compaction after buffer flush");
            perform_compaction(level);
        }
    }

    void LSMTree::perform_compaction(int level)
    {
        // Skip compaction if disabled
        if (!constants::COMPACTION_ENABLED.load())
        {
            log_debug("Compaction is disabled, skipping compaction of level " + std::to_string(level));
            return;
        }

        log_debug("Performing compaction on level " + std::to_string(level));

        // Get the compaction strategy for this level
        CompactionStrategy strategy = levels[level]->get_strategy();

        // Collect all key-value pairs from the runs in this level
        std::vector<KeyValuePair> all_pairs;
        const auto &runs = levels[level]->get_runs();

        for (const auto &run : runs)
        {
            auto pairs = run->get_all_pairs();
            all_pairs.insert(all_pairs.end(), pairs.begin(), pairs.end());
        }

        if (all_pairs.empty())
        {
            log_debug("No data to compact in level " + std::to_string(level));
            return;
        }

        // Sort by key (most recent values will come last if duplicates exist)
        std::sort(all_pairs.begin(), all_pairs.end());

        // Deduplicate keeping only the most recent value for each key
        // Since we might have read the pairs from multiple runs, there could be duplicates
        // We want to keep only the newest value (which should be the last one after sorting)
        std::vector<KeyValuePair> deduplicated;
        deduplicated.reserve(all_pairs.size());

        int64_t current_key = all_pairs[0].key;
        int64_t current_value = all_pairs[0].value;

        // Take the last value for each key (most recent from newest run)
        for (size_t i = 1; i < all_pairs.size(); ++i)
        {
            if (all_pairs[i].key == current_key)
            {
                // Found a duplicate, update to newer value
                current_value = all_pairs[i].value;
            }
            else
            {
                // Found a new key, store the previous one if not a tombstone
                if (current_value != INT64_MIN)
                {
                    deduplicated.emplace_back(current_key, current_value);
                }
                // Start tracking the new key
                current_key = all_pairs[i].key;
                current_value = all_pairs[i].value;
            }
        }

        // Add the last key if not a tombstone
        if (current_value != INT64_MIN)
        {
            deduplicated.emplace_back(current_key, current_value);
        }

        // Replace all_pairs with properly deduplicated data
        all_pairs = std::move(deduplicated);

        // Calculate the total size of merged data
        size_t total_size = all_pairs.size() * (sizeof(int64_t) * 2);

        log_debug("Compacted " + std::to_string(runs.size()) + " runs into " +
                  std::to_string(all_pairs.size()) + " key-value pairs (" +
                  std::to_string(total_size) + " bytes)");

        // Strategy-specific handling
        switch (strategy)
        {
        case CompactionStrategy::TIERING:
            // In TIERING, only move to next level when threshold is reached
            {
                int next_level = level + 1;

                if (runs.size() >= constants::TIERING_THRESHOLD)
                {
                    size_t run_id = levels[next_level]->run_count();
                    double fpr = calculate_fpr_for_level(next_level);

                    // Create a new run in the next level
                    if (!all_pairs.empty())
                    {
                        auto new_run = std::make_unique<Run>(all_pairs, next_level, run_id, fpr);
                        levels[next_level]->add_run(std::move(new_run));
                        log_debug("TIERING: Moved data from level " + std::to_string(level) +
                                  " to level " + std::to_string(next_level) +
                                  " after reaching threshold of " + std::to_string(constants::TIERING_THRESHOLD) + " runs");

                        // Clear this level only after successful compaction
                        levels[level]->clear_runs();
                        log_debug("TIERING: Cleared runs from level " + std::to_string(level) +
                                  " after successful compaction");

                        // Check if the next level needs compaction
                        if (levels[next_level]->needs_compaction())
                        {
                            log_debug("TIERING: Level " + std::to_string(next_level) +
                                      " needs compaction after receiving data from level " +
                                      std::to_string(level));
                            perform_compaction(next_level);
                        }
                    }
                    else
                    {
                        // All data was tombstones, just clear the runs
                        levels[level]->clear_runs();
                        log_debug("TIERING: Cleared runs from level " + std::to_string(level) +
                                  " (all data was tombstones)");
                    }
                }
                else
                {
                    log_debug("TIERING: Not enough runs for compaction in level " +
                              std::to_string(level) + ". Current: " + std::to_string(runs.size()) +
                              ", Threshold: " + std::to_string(constants::TIERING_THRESHOLD));
                }

                // Check if we need to extend levels (when adding to highest level)
                if (next_level == max_level && levels[next_level]->run_count() > 0)
                {
                    check_and_extend_levels();
                }
            }
            break;

        case CompactionStrategy::LAZY_LEVELING:
            // In LAZY_LEVELING, compact in place OR move to next level if too large
            {
                // Check if the run is too large for this level
                int target_level = get_target_level_for_size(total_size);

                if (target_level > level && !all_pairs.empty())
                {
                    // The compacted data is too large, move it to a deeper level
                    size_t run_id = levels[target_level]->run_count();
                    double fpr = calculate_fpr_for_level(target_level);

                    auto new_run = std::make_unique<Run>(all_pairs, target_level, run_id, fpr);
                    levels[target_level]->add_run(std::move(new_run));

                    log_debug("Lazy leveling: Moved data from level " + std::to_string(level) +
                              " to level " + std::to_string(target_level) +
                              " due to size considerations");

                    // Clear existing runs in this level
                    levels[level]->clear_runs();

                    // Check if the target level needs compaction
                    if (levels[target_level]->needs_compaction())
                    {
                        perform_compaction(target_level);
                    }
                }
                else if (!all_pairs.empty())
                {
                    // Compact in place (default LAZY_LEVELING behavior)
                    // Clear existing runs
                    levels[level]->clear_runs();

                    // Create a new single run
                    size_t run_id = 0;
                    double fpr = calculate_fpr_for_level(level);

                    auto new_run = std::make_unique<Run>(all_pairs, level, run_id, fpr);
                    levels[level]->add_run(std::move(new_run));

                    log_debug("Lazy leveling: Compacted runs in place at level " +
                              std::to_string(level));
                }
                else
                {
                    // All data was tombstones, just clear the runs
                    levels[level]->clear_runs();
                }

                // If this is the highest level, check if we need to extend
                if (level == max_level && levels[level]->run_count() > 0)
                {
                    check_and_extend_levels();
                }
            }
            break;

        case CompactionStrategy::LEVELING:
            // Move down if too large, otherwise compact in place
            {
                // Estimate total size
                int target_level = get_target_level_for_size(total_size);

                if (target_level > level && !all_pairs.empty())
                {
                    // Move to a deeper level
                    size_t run_id = levels[target_level]->run_count();
                    double fpr = calculate_fpr_for_level(target_level);

                    auto new_run = std::make_unique<Run>(all_pairs, target_level, run_id, fpr);
                    levels[target_level]->add_run(std::move(new_run));

                    log_debug("Leveling: Moved data from level " + std::to_string(level) +
                              " to level " + std::to_string(target_level) +
                              " due to size considerations");

                    // Clear existing runs in this level
                    levels[level]->clear_runs();

                    // Check if the target level needs compaction
                    if (levels[target_level]->needs_compaction())
                    {
                        perform_compaction(target_level);
                    }
                }
                else if (!all_pairs.empty())
                {
                    // Compact in place
                    levels[level]->clear_runs();

                    size_t run_id = 0;
                    double fpr = calculate_fpr_for_level(level);

                    auto new_run = std::make_unique<Run>(all_pairs, level, run_id, fpr);
                    levels[level]->add_run(std::move(new_run));

                    log_debug("Leveling: Compacted runs in place at level " +
                              std::to_string(level));
                }
                else
                {
                    // All data was tombstones, just clear the runs
                    levels[level]->clear_runs();
                }

                // If this is the highest level, check if we need to extend
                if (level == max_level && levels[level]->run_count() > 0)
                {
                    check_and_extend_levels();
                }
            }
            break;
        }

        log_debug("Compaction on level " + std::to_string(level) + " completed");
    }

    CompactionStrategy LSMTree::get_strategy_for_level(int level) const
    {
        if (level == 1)
        {
            return CompactionStrategy::TIERING;
        }
        else if (level >= 2 && level <= 4)
        {
            return CompactionStrategy::LAZY_LEVELING;
        }
        else
        {
            return CompactionStrategy::LEVELING;
        }
    }

    double LSMTree::calculate_fpr_for_level(int level) const
    {
        if (level == 0)
        {
            // Buffer doesn't use bloom filter
            return 1.0;
        }

        // Calculate FPR based on the Monkey formula
        // FPR_i = r / T^(L-i)
        double r = constants::TOTAL_FPR;
        double T = static_cast<double>(constants::SIZE_RATIO);
        int L = max_level.load();

        double fpr = r / std::pow(T, L - level);

        // Cap at 1.0 for safety
        return std::min(fpr, 1.0);
    }

    int LSMTree::get_target_level_for_size(size_t size_bytes) const
    {
        // Calculate which level this size belongs to
        // Level capacity = BUFFER_SIZE * SIZE_RATIO^(level-1)
        double buffer_size = static_cast<double>(constants::BUFFER_SIZE_BYTES.load());
        double size_ratio = static_cast<double>(constants::SIZE_RATIO);

        // Start at level 1
        int level = 1;
        double level_capacity = buffer_size * size_ratio;

        while (level < max_level && size_bytes > level_capacity)
        {
            level++;
            level_capacity *= size_ratio;
        }

        return level;
    }

    void LSMTree::check_and_extend_levels()
    {
        // Check if we need to add more levels
        int current_max_level = max_level.load();

        // If the deepest level has runs, we might need to extend
        if (!levels.empty() && levels.back()->run_count() > 0)
        {
            log_debug("Adding a new level to the LSM-tree");

            int new_level = current_max_level + 1;

            // Add a new level with LEVELING strategy
            levels.push_back(std::make_unique<Level>(new_level, CompactionStrategy::LEVELING));

            // Update max level
            max_level.store(new_level);

            // Recalculate FPRs and rebuild bloom filters
            rebuild_filters();
        }
    }

    void LSMTree::load_state_from_disk()
    {
        log_debug("Loading LSM-tree state from disk");

        // Check if data directory exists
        if (!fs::exists(constants::DATA_DIRECTORY))
        {
            log_debug("Data directory doesn't exist, nothing to load");
            return;
        }

        // Map to collect runs by level
        std::map<int, std::vector<std::pair<size_t, std::string>>> level_runs;

        // Scan for run files
        for (const auto &entry : fs::directory_iterator(constants::DATA_DIRECTORY))
        {
            std::string filename = entry.path().filename().string();

            // Parse run files: run_[level]_[id].data
            if (filename.find(constants::RUN_FILENAME_PREFIX) == 0 &&
                filename.size() > 5 &&
                filename.substr(filename.size() - 5) == ".data")
            {

                // Extract level and id from filename
                auto pos1 = filename.find('_') + 1;
                auto pos2 = filename.find('_', pos1);
                auto pos3 = filename.find('.', pos2);

                if (pos1 != std::string::npos && pos2 != std::string::npos && pos3 != std::string::npos)
                {
                    int level = std::stoi(filename.substr(pos1, pos2 - pos1));
                    size_t id = std::stoull(filename.substr(pos2 + 1, pos3 - pos2 - 1));

                    level_runs[level].emplace_back(id, entry.path().string());
                }
            }
        }

        // Load runs into the tree
        for (const auto &[level, runs] : level_runs)
        {
            // Make sure we have enough levels
            while (static_cast<size_t>(level) >= levels.size())
            {
                CompactionStrategy strategy = get_strategy_for_level(levels.size());
                levels.push_back(std::make_unique<Level>(levels.size(), strategy));
            }

            // Sort runs by ID to load them in order
            auto sorted_runs = runs;
            std::sort(sorted_runs.begin(), sorted_runs.end(),
                      [](const auto &a, const auto &b)
                      { return a.first < b.first; });

            // Load each run
            for (const auto &[id, filename] : sorted_runs)
            {
                try
                {
                    auto run = std::make_unique<Run>(filename, level, id);
                    levels[level]->add_run(std::move(run));
                    log_debug("Loaded run " + std::to_string(id) + " from level " + std::to_string(level));
                }
                catch (const std::exception &e)
                {
                    log_debug("Failed to load run: " + std::string(e.what()));
                }
            }
        }

        // After loading, check if any levels need compaction
        for (size_t i = 1; i < levels.size(); ++i)
        {
            if (levels[i]->needs_compaction())
            {
                log_debug("Level " + std::to_string(i) + " needs compaction after loading state");
                perform_compaction(i);
            }
        }

        log_debug("Finished loading LSM-tree state from disk");
    }

    void LSMTree::log_debug(const std::string &message) const
    {
        // For shutdown or initialization messages, use a simpler format
        if (message.find("shutdown") != std::string::npos ||
            message.find("initialized") != std::string::npos ||
            message.find("Loading") != std::string::npos ||
            message.find("Finished loading") != std::string::npos ||
            message.find("Flushing buffer during shutdown") != std::string::npos)
        {
            std::cout << message << std::endl;
            return;
        }

        // Regular operational logs get timestamps
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << "[" << std::ctime(&time) << "] " << message;

        // Remove newline from ctime output
        std::string log_message = ss.str();
        if (!log_message.empty() && log_message[log_message.size() - 1] == '\n')
        {
            log_message.erase(log_message.size() - 1);
        }

        std::cout << log_message << std::endl;
    }

    // Helper method to convert CompactionStrategy enum to string for logging
    std::string LSMTree::get_strategy_name(CompactionStrategy strategy) const
    {
        switch (strategy)
        {
        case CompactionStrategy::TIERING:
            return "TIERING";
        case CompactionStrategy::LAZY_LEVELING:
            return "LAZY_LEVELING";
        case CompactionStrategy::LEVELING:
            return "LEVELING";
        default:
            return "UNKNOWN";
        }
    }

    // Buffer size management
    size_t LSMTree::get_buffer_size() const
    {
        return constants::BUFFER_SIZE_BYTES.load();
    }

    void LSMTree::set_buffer_size(size_t new_size)
    {
        log_debug("Changing buffer size from " + std::to_string(constants::BUFFER_SIZE_BYTES.load()) +
                  " to " + std::to_string(new_size) + " bytes");
        constants::BUFFER_SIZE_BYTES.store(new_size);
    }

    // Compaction control
    bool LSMTree::is_compaction_enabled() const
    {
        return constants::COMPACTION_ENABLED.load();
    }

    void LSMTree::set_compaction_enabled(bool enabled)
    {
        log_debug(enabled ? "Enabling compaction" : "Disabling compaction");
        constants::COMPACTION_ENABLED.store(enabled);
    }

    // Optimized bulk loading
    void LSMTree::bulk_load_file(const std::string &filepath)
    {
        // Remember original settings to restore later
        size_t original_buffer_size = get_buffer_size();
        bool original_compaction_state = is_compaction_enabled();

        // Create a unique_lock instead of lock_guard so we can manually unlock it
        std::unique_lock<std::mutex> lock(tree_mutex);

        try
        {
            log_debug("Starting bulk load from file: " + filepath);

            // 1. Optimize settings for loading
            set_buffer_size(100 * 1024 * 1024); // 100MB buffer
            set_compaction_enabled(false);      // Disable compaction during load

            // 2. Open the input file
            std::ifstream file(filepath, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to open file: " + filepath);
            }

            // 3. First pass: count pairs and calculate total size
            std::ifstream count_file(filepath, std::ios::binary);
            size_t total_pairs = 0;
            int64_t key, value;
            while (count_file.read(reinterpret_cast<char *>(&key), sizeof(key)) &&
                   count_file.read(reinterpret_cast<char *>(&value), sizeof(value)))
            {
                total_pairs++;
            }

            log_debug("Bulk loading " + std::to_string(total_pairs) + " pairs from file");
            size_t data_size_bytes = total_pairs * sizeof(KeyValuePair);

            // 4. Load data in large chunks directly into memory
            // We can now pre-allocate memory for all pairs
            std::vector<KeyValuePair> all_pairs;
            all_pairs.reserve(total_pairs);

            // Reset file position
            file.clear();
            file.seekg(0);

            // Read all pairs
            while (file.read(reinterpret_cast<char *>(&key), sizeof(key)) &&
                   file.read(reinterpret_cast<char *>(&value), sizeof(value)))
            {
                all_pairs.emplace_back(key, value);
            }

            // 5. Sort all pairs
            log_debug("Sorting " + std::to_string(all_pairs.size()) + " pairs");
            std::sort(all_pairs.begin(), all_pairs.end());

            // 6. Deduplicate to keep only the latest value for each key
            if (all_pairs.size() > 1)
            {
                std::vector<KeyValuePair> deduplicated;
                deduplicated.reserve(all_pairs.size());

                int64_t current_key = all_pairs[0].key;
                int64_t current_value = all_pairs[0].value;

                for (size_t i = 1; i < all_pairs.size(); ++i)
                {
                    if (all_pairs[i].key == current_key)
                    {
                        // Found a duplicate, update to newer value
                        current_value = all_pairs[i].value;
                    }
                    else
                    {
                        // Found a new key, store the previous one if not a tombstone
                        if (current_value != INT64_MIN)
                        {
                            deduplicated.emplace_back(current_key, current_value);
                        }
                        // Start tracking the new key
                        current_key = all_pairs[i].key;
                        current_value = all_pairs[i].value;
                    }
                }

                // Add the last key if not a tombstone
                if (current_value != INT64_MIN)
                {
                    deduplicated.emplace_back(current_key, current_value);
                }

                // Replace with deduplicated data
                all_pairs = std::move(deduplicated);
            }

            // 7. Distribute data across levels working backwards
            size_t data_size_mb = data_size_bytes / (1024 * 1024); // Convert to MB for easier logging
            double default_buffer_mb = constants::DEFAULT_BUFFER_SIZE_BYTES / (1024.0 * 1024.0);
            double size_ratio = static_cast<double>(constants::SIZE_RATIO);

            log_debug("Distributing " + std::to_string(data_size_mb) + "MB of data across levels");

            // Calculate capacity for each level in MB
            std::vector<double> level_capacities_mb;
            for (int level = 1; level <= max_level; level++)
            {
                double capacity = default_buffer_mb * std::pow(size_ratio, level - 1);
                level_capacities_mb.push_back(capacity);
                log_debug("Level " + std::to_string(level) + " capacity: " +
                          std::to_string(capacity) + "MB");
            }

            // Find the lowest level that can hold all the data
            int target_level = 1;
            while (target_level <= max_level && level_capacities_mb[target_level - 1] < data_size_mb)
            {
                target_level++;
            }

            // If we couldn't find a level with enough capacity, use the highest level
            if (target_level > max_level)
            {
                target_level = max_level;
            }

            log_debug("Lowest level that can hold all data: " + std::to_string(target_level));

            // Calculate level distribution working backwards from target_level
            std::vector<double> level_data_mb(max_level, 0.0);
            double remaining_data_mb = data_size_mb;

            for (int level = target_level; level > 0 && remaining_data_mb > 0; level--)
            {
                // Get capacity of the previous level (for flushing calculation)
                double prev_level_capacity = (level > 1) ? level_capacities_mb[level - 2] : default_buffer_mb;

                // Calculate how many times we would have flushed from the previous level
                int flush_count = static_cast<int>(remaining_data_mb / prev_level_capacity);
                double data_for_level = flush_count * prev_level_capacity;

                // Don't allocate more than the remaining data
                data_for_level = std::min(data_for_level, remaining_data_mb);

                // Store the allocation
                level_data_mb[level - 1] = data_for_level;
                remaining_data_mb -= data_for_level;

                log_debug("Level " + std::to_string(level) + " gets " +
                          std::to_string(data_for_level) + "MB (" +
                          std::to_string(flush_count) + " flushes from level " +
                          std::to_string(level - 1) + ")");
            }

            // Distribute any remaining data to level 1
            if (remaining_data_mb > 0)
            {
                level_data_mb[0] += remaining_data_mb;
                log_debug("Level 1 gets additional " + std::to_string(remaining_data_mb) +
                          "MB of remaining data");
            }

            // Distribute the actual pairs according to the calculated allocations
            size_t pairs_per_mb = all_pairs.size() / data_size_mb;
            size_t start_idx = 0;

            for (int level = 1; level <= max_level; level++)
            {
                if (level_data_mb[level - 1] <= 0)
                {
                    continue; // Skip levels with no allocation
                }

                // Calculate number of pairs for this level
                size_t pair_count = static_cast<size_t>(level_data_mb[level - 1] * pairs_per_mb);

                // Ensure we don't exceed available pairs
                pair_count = std::min(pair_count, all_pairs.size() - start_idx);

                if (pair_count > 0)
                {
                    // Create a vector slice for this level
                    std::vector<KeyValuePair> level_pairs(
                        all_pairs.begin() + start_idx,
                        all_pairs.begin() + start_idx + pair_count);

                    // Create run for this level
                    size_t run_id = levels[level]->run_count();
                    double fpr = calculate_fpr_for_level(level);

                    log_debug("Creating run with " + std::to_string(level_pairs.size()) +
                              " pairs in level " + std::to_string(level) +
                              " (" + std::to_string(level_data_mb[level - 1]) + "MB)");

                    auto new_run = std::make_unique<Run>(level_pairs, level, run_id, fpr);
                    levels[level]->add_run(std::move(new_run));

                    // Update start index for next level
                    start_idx += pair_count;
                }
            }

            if (start_idx < all_pairs.size())
            {
                size_t remaining_pairs = all_pairs.size() - start_idx;
                log_debug("Warning: " + std::to_string(remaining_pairs) +
                          " pairs weren't distributed to any level");

                // Put remaining pairs in the highest level that received data
                int highest_used_level = max_level;
                while (highest_used_level > 0 && level_data_mb[highest_used_level - 1] <= 0)
                {
                    highest_used_level--;
                }

                if (highest_used_level > 0)
                {
                    std::vector<KeyValuePair> remaining_level_pairs(
                        all_pairs.begin() + start_idx,
                        all_pairs.end());

                    size_t run_id = levels[highest_used_level]->run_count();
                    double fpr = calculate_fpr_for_level(highest_used_level);

                    log_debug("Adding remaining " + std::to_string(remaining_level_pairs.size()) +
                              " pairs to level " + std::to_string(highest_used_level));

                    auto new_run = std::make_unique<Run>(remaining_level_pairs, highest_used_level, run_id, fpr);
                    levels[highest_used_level]->add_run(std::move(new_run));
                }
            }

            log_debug("Bulk load completed successfully");

            // Unlock the mutex before starting compaction
            // This ensures other threads can access the tree while compaction runs
            lock.unlock();

            // 9. Perform a full compaction after load is complete
            set_compaction_enabled(true);
            compact();
        }
        catch (const std::exception &e)
        {
            // Restore original settings before propagating the exception
            set_buffer_size(original_buffer_size);
            set_compaction_enabled(original_compaction_state);

            // Make sure to unlock if we're still holding the lock
            if (lock.owns_lock())
            {
                lock.unlock();
            }

            log_debug("Bulk load failed: " + std::string(e.what()));
            throw;
        }

        // 10. Restore original settings
        set_buffer_size(original_buffer_size);

        // No need to unlock here - either we've already unlocked above
        // or the exception handler did it

        log_debug("Bulk load fully completed, ready for normal operations");
    }

    // I/O statistics tracking
    void LSMTree::increment_read_io() { read_io_count++; }
    void LSMTree::increment_write_io() { write_io_count++; }
    size_t LSMTree::get_read_io_count() const { return read_io_count.load(); }
    size_t LSMTree::get_write_io_count() const { return write_io_count.load(); }
    void LSMTree::reset_io_stats()
    {
        read_io_count.store(0);
        write_io_count.store(0);
    }

    // Operation timing metrics implementation
    double LSMTree::get_avg_read_time_ms() const
    {
        size_t count = read_count.load();
        return count > 0 ? total_read_time_ms.load() / count : 0.0;
    }

    double LSMTree::get_avg_write_time_ms() const
    {
        size_t count = write_count.load();
        return count > 0 ? total_write_time_ms.load() / count : 0.0;
    }

    size_t LSMTree::get_read_count() const
    {
        return read_count.load();
    }

    size_t LSMTree::get_write_count() const
    {
        return write_count.load();
    }

    void LSMTree::reset_timing_stats()
    {
        read_count.store(0);
        write_count.store(0);
        total_read_time_ms.store(0.0);
        total_write_time_ms.store(0.0);
    }

} // namespace lsm