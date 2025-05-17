#include "../include/run.h"
#include "../include/constants.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace lsm
{

    Run::Run(const std::vector<KeyValuePair> &data, int level, size_t run_id, double fpr)
        : level(level), run_id(run_id), num_pairs(data.size()), bytes(0)
    {

        // Create filename
        filename = constants::DATA_DIRECTORY + "/" +
                   constants::RUN_FILENAME_PREFIX +
                   std::to_string(level) + "_" +
                   std::to_string(run_id) + ".data";

        // Write data to disk
        write_to_disk(data);

        // Create metadata (bloom filter and fence pointers)
        create_metadata(data, fpr);
    }

    Run::Run(const std::string &filename, int level, size_t run_id)
        : level(level), run_id(run_id), filename(filename)
    {

        // Open the file to count pairs and calculate size
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file)
        {
            throw std::runtime_error("Failed to open run file: " + filename);
        }

        // Get file size
        bytes = file.tellg();

        // Calculate number of pairs (each pair is 2 int64_t values)
        num_pairs = bytes / (sizeof(int64_t) * 2);

        // Validate file size - must be a multiple of the pair size
        if (bytes % (sizeof(int64_t) * 2) != 0)
        {
            throw std::runtime_error("Invalid run file size for " + filename +
                                     ". Size: " + std::to_string(bytes) +
                                     " is not a multiple of " + std::to_string(sizeof(int64_t) * 2));
        }

        // Verify the file contains at least one key-value pair
        if (num_pairs == 0)
        {
            throw std::runtime_error("Empty run file: " + filename);
        }

        // Load metadata
        load_metadata();
    }

    std::optional<int64_t> Run::get(int64_t key) const
    {
        // Check bloom filter first
        if (bloom_filter && !bloom_filter->might_contain(key))
        {
            return std::nullopt; // Definitely not in the set
        }

        // Use fence pointers to find where to start looking
        size_t offset = fence_pointers ? fence_pointers->find_offset(key) : 0;

        // Open the file
        std::ifstream file(get_data_filename(), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open run file: " + get_data_filename());
        }

        // Seek to the offset
        file.seekg(offset);

        // Read pairs until we find the key or reach the end
        int64_t file_key, value;
        while (file.read(reinterpret_cast<char *>(&file_key), sizeof(file_key)) &&
               file.read(reinterpret_cast<char *>(&value), sizeof(value)))
        {
            if (file_key == key)
            {
                return value; // Found the key
            }
            if (file_key > key)
            {
                break; // Passed the key, it doesn't exist
            }
        }

        return std::nullopt; // Key not found
    }

    std::vector<KeyValuePair> Run::range(int64_t start_key, int64_t end_key) const
    {
        if (start_key >= end_key)
        {
            return {};
        }

        std::vector<KeyValuePair> results;

        // Use fence pointers to find the range to scan
        std::pair<size_t, size_t> offsets;
        if (fence_pointers)
        {
            offsets = fence_pointers->find_range_offsets(start_key, end_key);
        }
        else
        {
            offsets = std::pair<size_t, size_t>(0, std::numeric_limits<size_t>::max());
        }

        // Open the file
        std::ifstream file(get_data_filename(), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open run file: " + get_data_filename());
        }

        // Seek to the start offset
        file.seekg(offsets.first);

        // Read pairs until we find the range or reach the end
        int64_t file_key, value;
        while (file.read(reinterpret_cast<char *>(&file_key), sizeof(file_key)) &&
               file.read(reinterpret_cast<char *>(&value), sizeof(value)))
        {

            // Check if we've passed the end offset
            if (static_cast<size_t>(file.tellg()) > offsets.second && offsets.second != std::numeric_limits<size_t>::max())
            {
                break;
            }

            // Check if key is in range
            if (file_key >= start_key && file_key < end_key)
            {
                results.emplace_back(file_key, value);
            }

            // If we've passed the end key, we can stop
            if (file_key >= end_key)
            {
                break;
            }
        }

        return results;
    }

    size_t Run::size() const
    {
        return num_pairs;
    }

    size_t Run::size_bytes() const
    {
        return bytes;
    }

    int Run::get_level() const
    {
        return level;
    }

    size_t Run::get_run_id() const
    {
        return run_id;
    }

    std::string Run::get_filename() const
    {
        return filename;
    }

    size_t Run::get_bloom_filter_bits_per_element() const
    {
        if (bloom_filter)
        {
            return bloom_filter->bit_count() / num_pairs;
        }
        return 0;
    }

    void Run::rebuild_bloom_filter(double new_fpr)
    {
        // Read all data to rebuild the bloom filter
        auto all_pairs = get_all_pairs();

        // Create a new bloom filter
        bloom_filter = std::make_unique<BloomFilter>(new_fpr, all_pairs.size());

        // Insert all keys
        for (const auto &pair : all_pairs)
        {
            bloom_filter->insert(pair.key);
        }

        // Save the bloom filter
        bloom_filter->save(get_bloom_filter_filename());
    }

    void Run::save() const
    {
        // The data file is already written in the constructor or loaded

        // Save bloom filter and fence pointers if they exist
        if (bloom_filter)
        {
            bloom_filter->save(get_bloom_filter_filename());
        }

        if (fence_pointers)
        {
            fence_pointers->save(get_fence_pointers_filename());
        }
    }

    std::vector<KeyValuePair> Run::get_all_pairs() const
    {
        std::vector<KeyValuePair> pairs;
        pairs.reserve(num_pairs);

        // Open the file
        std::ifstream file(get_data_filename(), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open run file: " + get_data_filename());
        }

        // Read all pairs
        int64_t key, value;
        size_t count = 0;
        while (file.read(reinterpret_cast<char *>(&key), sizeof(key)) &&
               file.read(reinterpret_cast<char *>(&value), sizeof(value)))
        {
            pairs.emplace_back(key, value);
            count++;

            // Safety check to avoid reading past the expected number of pairs
            if (count >= num_pairs)
            {
                break;
            }
        }

        if (count != num_pairs)
        {
            std::cerr << "Warning: Expected " << num_pairs << " pairs but read " << count
                      << " from file " << get_data_filename() << std::endl;
        }

        return pairs;
    }

    KeyValuePair Run::read_pair_at(std::ifstream &file, size_t offset) const
    {
        // Seek to the offset
        file.seekg(offset);

        // Read the key and value
        int64_t key, value;
        if (file.read(reinterpret_cast<char *>(&key), sizeof(key)) &&
            file.read(reinterpret_cast<char *>(&value), sizeof(value)))
        {
            return {key, value};
        }

        throw std::runtime_error("Failed to read pair at offset: " + std::to_string(offset));
    }

    void Run::write_to_disk(const std::vector<KeyValuePair> &data) const
    {
        // Skip if no data to write
        if (data.empty())
        {
            std::cerr << "Warning: Attempting to write empty run to disk: "
                      << get_data_filename() << std::endl;
            return;
        }

        // Create parent directories if they don't exist
        fs::path path(get_data_filename());
        fs::create_directories(path.parent_path());

        // Open the file
        std::ofstream file(get_data_filename(), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to create run file: " + get_data_filename());
        }

        // Write all pairs
        for (const auto &pair : data)
        {
            file.write(reinterpret_cast<const char *>(&pair.key), sizeof(pair.key));
            file.write(reinterpret_cast<const char *>(&pair.value), sizeof(pair.value));
        }

        if (!file)
        {
            throw std::runtime_error("Failed to write data to run file: " + get_data_filename());
        }

        // Ensure all data is flushed to disk
        file.flush();
    }

    void Run::create_metadata(const std::vector<KeyValuePair> &data, double fpr)
    {
        // Create bloom filter
        bloom_filter = std::make_unique<BloomFilter>(fpr, data.size());

        // Insert all keys into the bloom filter
        for (const auto &pair : data)
        {
            bloom_filter->insert(pair.key);
        }

        // Create fence pointers
        std::vector<std::pair<int64_t, size_t>> key_offsets;
        key_offsets.reserve(data.size());

        size_t offset = 0;
        for (const auto &pair : data)
        {
            key_offsets.emplace_back(pair.key, offset);
            offset += sizeof(int64_t) * 2; // Key + value
        }

        fence_pointers = std::make_unique<FencePointers>(get_data_filename(), key_offsets);

        // Save metadata
        bloom_filter->save(get_bloom_filter_filename());
        fence_pointers->save(get_fence_pointers_filename());
    }

    void Run::load_metadata()
    {
        // Try to load bloom filter
        try
        {
            bloom_filter = std::make_unique<BloomFilter>(get_bloom_filter_filename());
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Failed to load bloom filter: " << e.what() << std::endl;
            bloom_filter = nullptr;
        }

        // Try to load fence pointers
        try
        {
            fence_pointers = std::make_unique<FencePointers>(get_fence_pointers_filename());
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Failed to load fence pointers: " << e.what() << std::endl;
            fence_pointers = nullptr;
        }
    }

    std::string Run::get_data_filename() const
    {
        return filename;
    }

    std::string Run::get_bloom_filter_filename() const
    {
        return get_data_filename() + ".bloom";
    }

    std::string Run::get_fence_pointers_filename() const
    {
        return get_data_filename() + ".fence";
    }

    bool Run::has_bloom_filter() const
    {
        return bloom_filter != nullptr;
    }

    bool Run::might_contain(int64_t key) const
    {
        if (!bloom_filter)
        {
            return true; // No bloom filter, key might be present
        }
        return bloom_filter->might_contain(key);
    }

    std::vector<KeyValuePair> Run::get_sample_pairs(size_t max_count) const
    {
        // If max_count is zero or greater than num_pairs, limit to a smaller number
        size_t limit = std::min(max_count, num_pairs);
        if (limit == 0)
        {
            return {};
        }

        std::vector<KeyValuePair> sample_pairs;
        sample_pairs.reserve(limit);

        // Open the file
        std::ifstream file(get_data_filename(), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open run file: " + get_data_filename());
        }

        // Read the limited number of pairs from the beginning of the file
        int64_t key, value;
        size_t count = 0;
        while (count < limit &&
               file.read(reinterpret_cast<char *>(&key), sizeof(key)) &&
               file.read(reinterpret_cast<char *>(&value), sizeof(value)))
        {
            sample_pairs.emplace_back(key, value);
            count++;
        }

        return sample_pairs;
    }

    Run::~Run()
    {
        // Destructor shouldn't delete files automatically since that could create issues
        // with persistent storage and recovery. Explicit removal should be done via delete_files_from_disk()
    }

    void Run::delete_files_from_disk()
    {
        try
        {
            // Delete the main data file
            if (fs::exists(get_data_filename()))
            {
                fs::remove(get_data_filename());
                std::cout << "Deleted run file: " << get_data_filename() << std::endl;
            }

            // Delete the bloom filter file
            if (fs::exists(get_bloom_filter_filename()))
            {
                fs::remove(get_bloom_filter_filename());
                std::cout << "Deleted bloom filter file: " << get_bloom_filter_filename() << std::endl;
            }

            // Delete the fence pointers file
            if (fs::exists(get_fence_pointers_filename()))
            {
                fs::remove(get_fence_pointers_filename());
                std::cout << "Deleted fence pointers file: " << get_fence_pointers_filename() << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error deleting files for run: " << e.what() << std::endl;
        }
    }

} // namespace lsm