#ifndef FENCE_POINTERS_H
#define FENCE_POINTERS_H

#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include <memory>

namespace lsm
{

    // Fence pointer implementation for efficient range queries
    class FencePointers
    {
    public:
        // Create fence pointers for a run file with keys at the given offsets
        FencePointers(const std::string &run_filename, const std::vector<std::pair<int64_t, size_t>> &key_offsets);

        // Load fence pointers from a file
        FencePointers(const std::string &fence_pointers_filename);

        // Find the offset in the data file where a key might be located
        // Returns the offset to start scanning from
        size_t find_offset(int64_t key) const;

        // Find the range of offsets to scan for a range query
        // Returns start and end offsets to scan
        std::pair<size_t, size_t> find_range_offsets(int64_t start_key, int64_t end_key) const;

        // Save fence pointers to a file
        void save(const std::string &filename) const;

        // Get number of fence pointers
        size_t size() const;

    private:
        // Each fence pointer contains a key and its offset in the data file
        struct FencePointer
        {
            int64_t key;
            size_t offset;

            FencePointer(int64_t k, size_t off) : key(k), offset(off) {}
        };

        // Sorted array of fence pointers
        std::vector<FencePointer> fence_pointers;

        // Reference to the run filename
        std::string run_filename;

        // Binary search to find the fence pointer for a key
        size_t binary_search(int64_t key) const;
    };

} // namespace lsm

#endif // FENCE_POINTERS_H