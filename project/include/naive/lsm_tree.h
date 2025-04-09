#ifndef NAIVE_LSM_TREE_H
#define NAIVE_LSM_TREE_H

#include <cstdint>
#include <memory>
#include <vector>
#include <optional>
#include <string>

namespace naive
{

    // Forward declarations
    class MemTable;
    class SSTable;

    /**
     * LSM-Tree - Log-Structured Merge Tree implementation
     *
     * This is a placeholder implementation that will be expanded later.
     */
    class LSMTree
    {
    public:
        using Key = int32_t;
        using Value = int32_t;

        /**
         * Constructor - Creates an empty LSM-Tree
         * @param data_dir Directory where SSTables will be stored
         */
        explicit LSMTree(const std::string &data_dir);

        /**
         * Destructor
         */
        ~LSMTree();

    private:
        // Will be implemented later
    };

} // namespace naive

#endif // NAIVE_LSM_TREE_H