#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include "../include/constants.h"

// Size of one key-value pair in bytes (two int64_t values)
constexpr size_t PAIR_SIZE = sizeof(int64_t) * 2;

// Calculate how many pairs fit in the buffer minus one
size_t calc_pairs_for_almost_full_buffer()
{
    // Max is BUFFER_SIZE_BYTES / PAIR_SIZE
    // one less than that to leave room for one more pair
    return (lsm::constants::BUFFER_SIZE_BYTES / PAIR_SIZE) - 1;
}

int main(int argc, char *argv[])
{
    // Default output file
    std::string output_file = "almost_full_buffer.bin";

    // Parse command line arguments
    if (argc > 1)
    {
        output_file = argv[1];
    }

    // Calculate number of pairs
    size_t num_pairs = calc_pairs_for_almost_full_buffer();
    size_t file_size = num_pairs * PAIR_SIZE;
    double file_size_mb = static_cast<double>(file_size) / (1024 * 1024);

    std::cout << "Generating binary file with " << num_pairs << " key-value pairs" << std::endl;
    std::cout << "This will use " << file_size << " bytes (" << file_size_mb << " MB)" << std::endl;
    std::cout << "Buffer capacity is " << lsm::constants::BUFFER_SIZE_BYTES << " bytes" << std::endl;
    std::cout << "Space remaining for one more pair: " << (lsm::constants::BUFFER_SIZE_BYTES - file_size) << " bytes" << std::endl;

    // Initialize random number generator
    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<int64_t> dist(1, 1000000);

    // Generate sorted keys
    std::vector<int64_t> keys(num_pairs);
    for (size_t i = 0; i < num_pairs; ++i)
    {
        keys[i] = i + 1;
    }

    // Open output file
    std::ofstream file(output_file, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error: Unable to open output file " << output_file << std::endl;
        return 1;
    }

    // Write pairs to file
    for (size_t i = 0; i < num_pairs; ++i)
    {
        int64_t key = keys[i];
        int64_t value = dist(rng);

        file.write(reinterpret_cast<const char *>(&key), sizeof(key));
        file.write(reinterpret_cast<const char *>(&value), sizeof(value));
    }

    file.close();

    std::cout << "File generation complete!" << std::endl;
    std::cout << "To test buffer flushing, load this file and then insert one more key-value pair." << std::endl;
    std::cout << "Use command: l \"" << output_file << "\"" << std::endl;
    std::cout << "Then insert: p " << (num_pairs + 1) << " 42" << std::endl;

    return 0;
}