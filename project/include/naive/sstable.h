#ifndef NAIVE_SSTABLE_H
#define NAIVE_SSTABLE_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace naive
{

    // Forward declaration
    class MemTable;

    /**
     * SSTable - Sorted String Table for on-disk storage of key-value pairs
     *
     * This is a placeholder implementation that will be expanded later.
     */
    class SSTable
    {
    public:
        using Key = int32_t;
        using Value = int32_t;

        /**
         * Constructor - Creates an empty SSTable
         */
        SSTable();

        /**
         * Creates an SSTable from a MemTable by flushing it to disk
         * @param memtable The memtable to flush
         * @param file_path The path where the SSTable should be stored
         * @return True if the flush was successful, false otherwise
         */
        static bool create_from_memtable(const MemTable &memtable, const std::string &file_path);

    private:
        // Will be implemented later
    };

} // namespace naive

#endif // NAIVE_SSTABLE_H