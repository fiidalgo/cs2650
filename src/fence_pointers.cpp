#include "../include/fence_pointers.h"
#include "../include/constants.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace lsm
{

    FencePointers::FencePointers(const std::string &run_filename,
                                 const std::vector<std::pair<int64_t, size_t>> &key_offsets)
        : run_filename(run_filename)
    {

        // Reserve space for fence pointers
        fence_pointers.reserve(key_offsets.size());

        // Create fence pointers based on the page size
        size_t page_size = constants::PAGE_SIZE;
        size_t current_page = 0;

        for (const auto &[key, offset] : key_offsets)
        {
            size_t page_number = offset / page_size;

            if (page_number > current_page || fence_pointers.empty())
            {
                fence_pointers.emplace_back(key, offset);
                current_page = page_number;
            }
        }
    }

    FencePointers::FencePointers(const std::string &fence_pointers_filename)
    {
        std::ifstream file(fence_pointers_filename, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open fence pointers file: " + fence_pointers_filename);
        }

        // Read run filename length and then the filename
        size_t filename_length;
        file.read(reinterpret_cast<char *>(&filename_length), sizeof(filename_length));

        run_filename.resize(filename_length);
        file.read(&run_filename[0], filename_length);

        // Read number of fence pointers
        size_t count;
        file.read(reinterpret_cast<char *>(&count), sizeof(count));

        // Read all fence pointers
        fence_pointers.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            int64_t key;
            size_t offset;

            file.read(reinterpret_cast<char *>(&key), sizeof(key));
            file.read(reinterpret_cast<char *>(&offset), sizeof(offset));

            fence_pointers.emplace_back(key, offset);
        }

        if (!file)
        {
            throw std::runtime_error("Failed to read fence pointers from file: " + fence_pointers_filename);
        }
    }

    size_t FencePointers::find_offset(int64_t key) const
    {
        if (fence_pointers.empty())
        {
            return 0; // Start from the beginning if no fence pointers
        }

        // Binary search to find the appropriate fence pointer
        size_t index = binary_search(key);

        // Return the offset of the fence pointer
        return fence_pointers[index].offset;
    }

    std::pair<size_t, size_t> FencePointers::find_range_offsets(int64_t start_key, int64_t end_key) const
    {
        if (fence_pointers.empty())
        {
            return {0, 0}; // No data
        }

        // Find the fence pointer for the start key
        size_t start_index = binary_search(start_key);
        size_t start_offset = fence_pointers[start_index].offset;

        // Find the fence pointer for the end key
        size_t end_index = binary_search(end_key);

        // If we're at the last fence pointer, read until the end of the file
        size_t end_offset = (end_index == fence_pointers.size() - 1) ? std::numeric_limits<size_t>::max() : fence_pointers[end_index + 1].offset;

        return {start_offset, end_offset};
    }

    void FencePointers::save(const std::string &filename) const
    {
        std::ofstream file(filename, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to create fence pointers file: " + filename);
        }

        // Write run filename length and then the filename
        size_t filename_length = run_filename.length();
        file.write(reinterpret_cast<const char *>(&filename_length), sizeof(filename_length));
        file.write(run_filename.c_str(), filename_length);

        // Write number of fence pointers
        size_t count = fence_pointers.size();
        file.write(reinterpret_cast<const char *>(&count), sizeof(count));

        // Write all fence pointers
        for (const auto &fp : fence_pointers)
        {
            file.write(reinterpret_cast<const char *>(&fp.key), sizeof(fp.key));
            file.write(reinterpret_cast<const char *>(&fp.offset), sizeof(fp.offset));
        }

        if (!file)
        {
            throw std::runtime_error("Failed to write fence pointers to file: " + filename);
        }
    }

    size_t FencePointers::size() const
    {
        return fence_pointers.size();
    }

    size_t FencePointers::binary_search(int64_t key) const
    {
        // If key is less than the first fence pointer, return the first one
        if (key < fence_pointers.front().key)
        {
            return 0;
        }

        // If key is greater than or equal to the last fence pointer, return the last one
        if (key >= fence_pointers.back().key)
        {
            return fence_pointers.size() - 1;
        }

        // Binary search to find the largest fence pointer with key <= search_key
        size_t left = 0;
        size_t right = fence_pointers.size() - 1;

        while (left < right)
        {
            size_t mid = left + (right - left + 1) / 2; // Ceiling division

            if (fence_pointers[mid].key <= key)
            {
                left = mid;
            }
            else
            {
                right = mid - 1;
            }
        }

        return left;
    }

} // namespace lsm