#ifndef RUN_H
#define RUN_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <fstream>
#include <cstdint>

#include "bloom_filter.h"
#include "fence_pointers.h"
#include "lsm_tree.h"

namespace lsm
{

    // Represents a sorted run of key-value pairs
    class Run
    {
    public:
        // Create a new run from a vector of key-value pairs
        Run(const std::vector<KeyValuePair> &data, int level, size_t run_id, double fpr);

        // Load an existing run from disk
        Run(const std::string &filename, int level, size_t run_id);

        // Destructor
        ~Run();

        // Get the value associated with a key
        std::optional<int64_t> get(int64_t key) const;

        // Get all key-value pairs in a range [start_key, end_key)
        std::vector<KeyValuePair> range(int64_t start_key, int64_t end_key) const;

        // Check if this run has a bloom filter
        bool has_bloom_filter() const;

        // Check if a key might be in this run using the bloom filter
        bool might_contain(int64_t key) const;

        // Get the number of key-value pairs in the run
        size_t size() const;

        // Get the size of the run in bytes
        size_t size_bytes() const;

        // Get the level of this run
        int get_level() const;

        // Get the run ID
        size_t get_run_id() const;

        // Get the filename of this run
        std::string get_filename() const;

        // Get the bloom filter bits per element
        size_t get_bloom_filter_bits_per_element() const;

        // Rebuild the bloom filter with a new FPR
        void rebuild_bloom_filter(double new_fpr);

        // Save all components of the run to disk
        void save() const;

        // Delete all files associated with this run
        void delete_files_from_disk();

        // Get all key-value pairs from the run
        std::vector<KeyValuePair> get_all_pairs() const;

        // Get a sample of key-value pairs (for display purposes)
        std::vector<KeyValuePair> get_sample_pairs(size_t max_count) const;

    private:
        // The level this run belongs to
        int level;

        // Unique ID of this run within its level
        size_t run_id;

        // Filename for this run's data
        std::string filename;

        // Number of key-value pairs in the run
        size_t num_pairs;

        // Size in bytes
        size_t bytes;

        // Bloom filter for faster lookups
        std::unique_ptr<BloomFilter> bloom_filter;

        // Fence pointers for range queries
        std::unique_ptr<FencePointers> fence_pointers;

        // Read a key-value pair from the file at the given offset
        KeyValuePair read_pair_at(std::ifstream &file, size_t offset) const;

        // Write the run to disk
        void write_to_disk(const std::vector<KeyValuePair> &data) const;

        // Create bloom filter and fence pointers
        void create_metadata(const std::vector<KeyValuePair> &data, double fpr);

        // Load metadata (bloom filter and fence pointers)
        void load_metadata();

        // Generate filenames for different components
        std::string get_data_filename() const;
        std::string get_bloom_filter_filename() const;
        std::string get_fence_pointers_filename() const;
    };

} // namespace lsm

#endif // RUN_H