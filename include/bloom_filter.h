#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <cmath>

namespace lsm
{

    // BloomFilter implementation with variable FPR (False Positive Rate)
    class BloomFilter
    {
    public:
        // Construct a bloom filter with specified false positive rate and expected number of elements
        BloomFilter(double false_positive_rate, size_t expected_elements);

        // Load a bloom filter from file
        BloomFilter(const std::string &filename);

        // Insert a key into the bloom filter
        void insert(int64_t key);

        // Check if a key might be in the set
        bool might_contain(int64_t key) const;

        // Save the bloom filter to a file
        void save(const std::string &filename) const;

        // Get the number of bits in the filter
        size_t bit_count() const;

        // Get the false positive rate
        double get_fpr() const;

        // Get the number of hash functions
        size_t hash_function_count() const;

    private:
        // The bit array
        std::vector<bool> bits;

        // Number of hash functions
        size_t num_hash_functions;

        // Target false positive rate
        double fpr;

        // Expected number of elements
        size_t expected_num_elements;

        // Calculate optimal number of bits and hash functions
        void calculate_parameters();

        // Generate hash for a key and a specific hash function index
        size_t hash(int64_t key, size_t hash_index) const;

        // FNV-1a hash function
        size_t fnv1a_hash(int64_t key) const;
    };

    // Calculate the optimal number of bits for a bloom filter
    // Based on the formula: m = -n * ln(p) / (ln(2)^2)
    inline size_t optimal_bits(size_t n, double p)
    {
        return static_cast<size_t>(std::ceil(-static_cast<double>(n) * std::log(p) / (std::log(2.0) * std::log(2.0))));
    }

    // Calculate the optimal number of hash functions
    // Based on the formula: k = (m/n) * ln(2)
    inline size_t optimal_hash_functions(size_t m, size_t n)
    {
        return static_cast<size_t>(std::ceil((static_cast<double>(m) / static_cast<double>(n)) * std::log(2.0)));
    }

    // Calculate the FPR given bits and elements
    // Based on the formula: p = (1 - e^(-k*n/m))^k
    inline double expected_fpr(size_t m, size_t n, size_t k)
    {
        return std::pow(1.0 - std::exp(-static_cast<double>(k * n) / static_cast<double>(m)), static_cast<double>(k));
    }

} // namespace lsm

#endif // BLOOM_FILTER_H