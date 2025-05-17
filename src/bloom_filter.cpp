#include "../include/bloom_filter.h"
#include "../include/constants.h"
#include <fstream>
#include <stdexcept>
#include <cassert>
#include <cstring>

namespace lsm
{

    BloomFilter::BloomFilter(double false_positive_rate, size_t expected_elements)
        : fpr(false_positive_rate), expected_num_elements(expected_elements)
    {

        calculate_parameters();
    }

    BloomFilter::BloomFilter(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open bloom filter file: " + filename);
        }

        // Read metadata
        file.read(reinterpret_cast<char *>(&fpr), sizeof(fpr));
        file.read(reinterpret_cast<char *>(&expected_num_elements), sizeof(expected_num_elements));
        file.read(reinterpret_cast<char *>(&num_hash_functions), sizeof(num_hash_functions));

        size_t bit_count;
        file.read(reinterpret_cast<char *>(&bit_count), sizeof(bit_count));

        // Resize the bit array
        bits.resize(bit_count);

        // Read the bits
        // Since vector<bool> is specialized, we need a temporary buffer
        size_t byte_count = (bit_count + 7) / 8;
        std::vector<uint8_t> buffer(byte_count, 0);

        file.read(reinterpret_cast<char *>(buffer.data()), byte_count);

        // Convert bytes to bits
        for (size_t i = 0; i < bit_count; ++i)
        {
            size_t byte_index = i / 8;
            size_t bit_index = i % 8;
            bits[i] = (buffer[byte_index] & (1 << bit_index)) != 0;
        }

        if (!file)
        {
            throw std::runtime_error("Failed to read bloom filter data from file: " + filename);
        }
    }

    void BloomFilter::insert(int64_t key)
    {
        for (size_t i = 0; i < num_hash_functions; ++i)
        {
            size_t index = hash(key, i);
            bits[index] = true;
        }
    }

    bool BloomFilter::might_contain(int64_t key) const
    {
        for (size_t i = 0; i < num_hash_functions; ++i)
        {
            size_t index = hash(key, i);
            if (!bits[index])
            {
                return false; // Definitely not in the set
            }
        }
        return true; // Might be in the set
    }

    void BloomFilter::save(const std::string &filename) const
    {
        std::ofstream file(filename, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to create bloom filter file: " + filename);
        }

        // Write metadata
        file.write(reinterpret_cast<const char *>(&fpr), sizeof(fpr));
        file.write(reinterpret_cast<const char *>(&expected_num_elements), sizeof(expected_num_elements));
        file.write(reinterpret_cast<const char *>(&num_hash_functions), sizeof(num_hash_functions));

        size_t bit_count = bits.size();
        file.write(reinterpret_cast<const char *>(&bit_count), sizeof(bit_count));

        // Write the bits
        // Since vector<bool> is specialized, we need a temporary buffer
        size_t byte_count = (bit_count + 7) / 8;
        std::vector<uint8_t> buffer(byte_count, 0);

        for (size_t i = 0; i < bit_count; ++i)
        {
            if (bits[i])
            {
                size_t byte_index = i / 8;
                size_t bit_index = i % 8;
                buffer[byte_index] |= (1 << bit_index);
            }
        }

        file.write(reinterpret_cast<const char *>(buffer.data()), byte_count);

        if (!file)
        {
            throw std::runtime_error("Failed to write bloom filter data to file: " + filename);
        }
    }

    size_t BloomFilter::bit_count() const
    {
        return bits.size();
    }

    double BloomFilter::get_fpr() const
    {
        return fpr;
    }

    size_t BloomFilter::hash_function_count() const
    {
        return num_hash_functions;
    }

    void BloomFilter::calculate_parameters()
    {
        // Calculate optimal number of bits based on the formula: m = -n * ln(p) / (ln(2)^2)
        size_t m = optimal_bits(expected_num_elements, fpr);

        // Calculate optimal number of hash functions: k = (m/n) * ln(2)
        num_hash_functions = optimal_hash_functions(m, expected_num_elements);

        // Ensure at least 1 hash function
        num_hash_functions = std::max(size_t(1), num_hash_functions);

        // Resize the bit array
        bits.resize(m, false);
    }

    size_t BloomFilter::hash(int64_t key, size_t hash_index) const
    {
        // Use double hashing to generate multiple hash functions
        // h_i(key) = (h1(key) + i * h2(key)) % m

        // First hash function: FNV-1a
        uint64_t h1 = fnv1a_hash(key);

        // Second hash function: modify h1 slightly
        uint64_t h2 = fnv1a_hash(~key);

        // Combine the two hashes
        return (h1 + hash_index * h2) % bits.size();
    }

    size_t BloomFilter::fnv1a_hash(int64_t key) const
    {
        // Convert key to bytes
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&key);
        size_t len = sizeof(key);

        uint64_t hash = constants::FNV_OFFSET_BASIS;

        for (size_t i = 0; i < len; ++i)
        {
            hash ^= bytes[i];
            hash *= constants::FNV_PRIME;
        }

        return hash;
    }

} // namespace lsm