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
        // The rest of cleanup is handled by unique_ptr destructors
    }

    void LSMTree::put(int64_t key, int64_t value)
    {
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
                      std::to_string(constants::BUFFER_SIZE_BYTES) + " bytes), flushing to disk");
            flush_buffer();
        }
        else
        {
            log_debug("PUT: Buffer not full yet, remaining capacity: " +
                      std::to_string(constants::BUFFER_SIZE_BYTES - buffer->size_bytes()) + " bytes");
        }
    }

    std::optional<int64_t> LSMTree::get(int64_t key)
    {
        log_debug("GET operation: Searching for key=" + std::to_string(key));

        // First check the buffer
        auto buffer_result = buffer->get(key);
        if (buffer_result.has_value())
        {
            log_debug("GET: Found key in buffer, value=" + std::to_string(*buffer_result));
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
                    return result;
                }
                log_debug("GET: Key not found in run " + std::to_string(run_idx) +
                          " of level " + std::to_string(level_num));
            }
        }

        log_debug("GET: Key not found in any level");
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
        log_debug("Flushing buffer to disk");

        // Get all pairs from buffer in sorted order
        auto pairs = buffer->get_all_sorted();

        if (pairs.empty())
        {
            log_debug("Buffer is empty, nothing to flush");
            return;
        }

        // Create a new run in level 1
        int level = 1;
        size_t run_id = levels[level]->run_count();
        double fpr = calculate_fpr_for_level(level);

        auto run = std::make_unique<Run>(pairs, level, run_id, fpr);
        levels[level]->add_run(std::move(run));

        // Clear the buffer
        buffer->clear();

        // Check if level 1 needs compaction after the flush
        if (levels[level]->needs_compaction())
        {
            log_debug("Level " + std::to_string(level) + " needs compaction after buffer flush");
            perform_compaction(level);
        }
    }

    void LSMTree::perform_compaction(int level)
    {
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
        size_t total_size = all_pairs.size() * (sizeof(int64_t) * 2); // key + value

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

                // Only proceed with compaction if we've reached the threshold
                // The needs_compaction() check already happened, but this ensures we don't
                // accidentally compact due to recursive calls
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
                    size_t run_id = 0; // Always use ID 0 for a single run
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

                    size_t run_id = 0; // Always use ID 0 for a single run in leveling
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
        double buffer_size = static_cast<double>(constants::BUFFER_SIZE_BYTES);
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

} // namespace lsm