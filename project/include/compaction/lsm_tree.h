#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <mutex>
#include <map>
#include <queue>
#include <functional>

#include "naive/memtable.h"
#include "naive/sstable.h"

namespace lsm_tree {

class CompactionLSMTree {
public:
    // Constructor with configurable parameters
    CompactionLSMTree(
        const std::string& data_dir,
        size_t sstable_threshold_L0 = 4,
        size_t size_ratio = 10,
        const std::string& compaction_policy = "leveling"
    );

    // Core operations
    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    void range(const std::string& start_key, const std::string& end_key, 
               std::function<void(const std::string&, const std::string&)> callback);
    void remove(const std::string& key);
    void flush();
    void compact();

    // Statistics and metrics
    std::string getStats() const;
    size_t getLevelSize(size_t level) const;
    size_t getSSTableCount(size_t level) const;
    double getCompactionFrequency() const;
    size_t getTotalBytesWritten() const;
    size_t getTotalBytesRead() const;

private:
    // Configuration
    const std::string data_dir_;
    const size_t sstable_threshold_L0_;
    const size_t size_ratio_;
    const std::string compaction_policy_;

    // Components
    std::unique_ptr<lsm::naive::MemTable> active_memtable_;
    std::vector<std::vector<std::unique_ptr<lsm::naive::SSTable>>> levels_;  // levels_[0] is L0
    std::mutex mutex_;

    // Metrics
    size_t total_bytes_written_ = 0;
    size_t total_bytes_read_ = 0;
    size_t compaction_count_ = 0;
    size_t total_operations_ = 0;

    // Helper methods
    void checkAndTriggerCompaction();
    void performCompaction(size_t level);
    void mergeSSTables(size_t level, const std::vector<std::unique_ptr<lsm::naive::SSTable>>& sstables);
    bool shouldCompactLevel(size_t level) const;
    size_t calculateTargetLevelSize(size_t level) const;
    void updateMetrics(size_t bytes_written, size_t bytes_read);
};

} // namespace lsm_tree 