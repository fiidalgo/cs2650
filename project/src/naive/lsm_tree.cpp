#include "naive/lsm_tree.h"
#include "naive/memtable.h"

namespace naive
{

    LSMTree::LSMTree(const std::string &data_dir)
        : data_dir_(data_dir), memtable_()
    {
        // For now, we just initialize the memtable
        // In the future, we'll also recover state from the data directory
    }

    LSMTree::~LSMTree()
    {
        // For the naive implementation, there's nothing to clean up
        // In the future, we'll add flush operations to ensure data is saved
    }

    void LSMTree::put(LSMTree::Key key, LSMTree::Value value)
    {
        // In this naive implementation, we just forward to the memtable
        memtable_.put(key, value);
    }

    std::optional<LSMTree::Value> LSMTree::get(LSMTree::Key key) const
    {
        // For now, we just check the memtable
        // In the future, we'll check SSTables if not found in memtable
        return memtable_.get(key);
    }

    bool LSMTree::remove(LSMTree::Key key)
    {
        // For now, just mark as deleted in the memtable
        return memtable_.remove(key);
    }

    std::vector<std::pair<LSMTree::Key, LSMTree::Value>> LSMTree::range(LSMTree::Key start_key, LSMTree::Key end_key) const
    {
        // For now, just return results from the memtable
        // In the future, we'll merge results from memtable and SSTables
        return memtable_.range(start_key, end_key);
    }

} // namespace naive