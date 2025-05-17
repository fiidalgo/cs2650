#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <unordered_set>

// Size of a key-value pair in bytes
constexpr size_t KV_PAIR_SIZE = sizeof(int64_t) * 2;

// Target size of 10GB in bytes
constexpr size_t TARGET_SIZE = 10ULL * 1024 * 1024 * 1024;

// Number of pairs to generate (approximately)
constexpr size_t NUM_PAIRS = TARGET_SIZE / KV_PAIR_SIZE;

// Batch size for writing to reduce memory usage
constexpr size_t BATCH_SIZE = 1000000; // 1 million pairs per batch

// Structure to hold a key-value pair
struct KeyValuePair
{
    int64_t key;
    int64_t value;
};

// Generate a batch of random key-value pairs
std::vector<KeyValuePair> generate_batch(std::mt19937_64 &rng,
                                         std::uniform_int_distribution<int64_t> &key_dist,
                                         std::uniform_int_distribution<int64_t> &value_dist,
                                         size_t batch_size,
                                         std::unordered_set<int64_t> &used_keys)
{

    std::vector<KeyValuePair> batch;
    batch.reserve(batch_size);

    for (size_t i = 0; i < batch_size; ++i)
    {
        // Generate a unique key
        int64_t key;
        do
        {
            key = key_dist(rng);
        } while (used_keys.count(key) > 0);

        used_keys.insert(key);

        // Generate a random value
        int64_t value = value_dist(rng);

        // Add to batch
        batch.push_back({key, value});
    }

    // Sort by key for LSM-Tree loading
    std::sort(batch.begin(), batch.end(),
              [](const KeyValuePair &a, const KeyValuePair &b)
              {
                  return a.key < b.key;
              });

    return batch;
}

int main(int argc, char *argv[])
{
    // Default output file
    std::string output_file = "test_data_10gb.bin";

    // Parse command line arguments
    if (argc > 1)
    {
        output_file = argv[1];
    }

    std::cout << "Generating approximately 10GB of test data to " << output_file << std::endl;

    // Initialize random number generator
    std::random_device rd;
    std::mt19937_64 rng(rd());

    // Use full range of int64_t for keys and values
    std::uniform_int_distribution<int64_t> key_dist(0, std::numeric_limits<int64_t>::max() / 2);
    std::uniform_int_distribution<int64_t> value_dist(-1000000, 1000000);

    // Track used keys to ensure uniqueness
    std::unordered_set<int64_t> used_keys;

    // Open output file
    std::ofstream file(output_file, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error: Unable to open output file " << output_file << std::endl;
        return 1;
    }

    // Calculate number of full batches
    size_t num_batches = NUM_PAIRS / BATCH_SIZE;
    size_t remaining_pairs = NUM_PAIRS % BATCH_SIZE;

    // Track progress
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t total_pairs_written = 0;

    // Generate and write full batches
    for (size_t batch = 0; batch < num_batches; ++batch)
    {
        // Generate a batch of random pairs
        auto pairs = generate_batch(rng, key_dist, value_dist, BATCH_SIZE, used_keys);

        // Write batch to file
        for (const auto &pair : pairs)
        {
            file.write(reinterpret_cast<const char *>(&pair.key), sizeof(pair.key));
            file.write(reinterpret_cast<const char *>(&pair.value), sizeof(pair.value));
        }

        total_pairs_written += pairs.size();

        // Print progress every 10%
        if (batch % (num_batches / 10) == 0 || batch == num_batches - 1)
        {
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               current_time - start_time)
                               .count();

            double progress = static_cast<double>(batch + 1) / num_batches * 100.0;
            double data_size_gb = static_cast<double>(total_pairs_written * KV_PAIR_SIZE) /
                                  (1024 * 1024 * 1024);

            std::cout << "Progress: " << progress << "% ("
                      << data_size_gb << " GB, "
                      << total_pairs_written << " pairs, "
                      << elapsed << " seconds)" << std::endl;
        }
    }

    // Handle remaining pairs if any
    if (remaining_pairs > 0)
    {
        auto pairs = generate_batch(rng, key_dist, value_dist, remaining_pairs, used_keys);

        for (const auto &pair : pairs)
        {
            file.write(reinterpret_cast<const char *>(&pair.key), sizeof(pair.key));
            file.write(reinterpret_cast<const char *>(&pair.value), sizeof(pair.value));
        }

        total_pairs_written += pairs.size();
    }

    file.close();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
                          end_time - start_time)
                          .count();

    double total_gb = static_cast<double>(total_pairs_written * KV_PAIR_SIZE) / (1024 * 1024 * 1024);

    std::cout << "Data generation complete!" << std::endl;
    std::cout << "Total data generated: " << total_gb << " GB" << std::endl;
    std::cout << "Total pairs generated: " << total_pairs_written << std::endl;
    std::cout << "Time taken: " << total_time << " seconds" << std::endl;

    return 0;
}