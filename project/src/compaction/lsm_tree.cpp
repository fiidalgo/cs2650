#include "compaction/lsm_tree.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

namespace lsm_tree {

CompactionLSMTree::CompactionLSMTree(
    const std::string& data_dir,
    size_t sstable_threshold_L0,
    size_t size_ratio,
    const std::string& compaction_policy
) : data_dir_(data_dir),
    sstable_threshold_L0_(sstable_threshold_L0),
    size_ratio_(size_ratio),
    compaction_policy_(compaction_policy) {
    
    // Create data directory if it doesn't exist
    std::filesystem::create_directories(data_dir_);
    
    // Initialize components
    active_memtable_ = std::make_unique<lsm::naive::MemTable>();
    levels_.push_back(std::vector<std::unique_ptr<lsm::naive::SSTable>>());  // L0
}

void CompactionLSMTree::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_memtable_->put(key, value);
    total_operations_++;
    checkAndTriggerCompaction();
}

std::optional<std::string> CompactionLSMTree::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check active memtable first
    auto result = active_memtable_->get(key);
    if (result) return result;
    
    // Check each level in order
    for (const auto& level : levels_) {
        for (const auto& sstable : level) {
            result = sstable->get(key);
            if (result) return result;
        }
    }
    
    return std::nullopt;
}

void CompactionLSMTree::range(
    const std::string& start_key,
    const std::string& end_key,
    std::function<void(const std::string&, const std::string&)> callback
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Collect all SSTables that might contain keys in the range
    std::vector<std::unique_ptr<lsm::naive::SSTable>*> relevant_sstables;
    
    // Add L0 SSTables in reverse order (newest first)
    for (auto it = levels_[0].rbegin(); it != levels_[0].rend(); ++it) {
        relevant_sstables.push_back(&(*it));
    }
    
    // Add other levels in order
    for (size_t i = 1; i < levels_.size(); ++i) {
        for (auto& sstable : levels_[i]) {
            relevant_sstables.push_back(&sstable);
        }
    }
    
    // Process range query
    active_memtable_->range(start_key, end_key, callback);
    for (auto* sstable_ptr : relevant_sstables) {
        (*sstable_ptr)->range(start_key, end_key, callback);
    }
}

void CompactionLSMTree::remove(const std::string& key) {
    put(key, "");  // Use empty string as tombstone
}

void CompactionLSMTree::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_memtable_->size() == 0) return;
    
    auto sstable = lsm::naive::SSTable::createFromMemTable(data_dir_, *active_memtable_);
    levels_[0].push_back(std::move(sstable));
    active_memtable_->clear();
    
    checkAndTriggerCompaction();
}

void CompactionLSMTree::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < levels_.size(); ++i) {
        if (shouldCompactLevel(i)) {
            performCompaction(i);
        }
    }
}

void CompactionLSMTree::checkAndTriggerCompaction() {
    if (levels_[0].size() >= sstable_threshold_L0_) {
        performCompaction(0);
    }
}

void CompactionLSMTree::performCompaction(size_t level) {
    if (level >= levels_.size() - 1) {
        levels_.push_back(std::vector<std::unique_ptr<lsm::naive::SSTable>>());
    }
    
    std::vector<std::unique_ptr<lsm::naive::SSTable>> to_compact;
    if (level == 0) {
        // Move all L0 SSTables to compaction
        to_compact = std::move(levels_[0]);
        levels_[0].clear();
    } else {
        // For other levels, compact if size exceeds target
        if (shouldCompactLevel(level)) {
            to_compact = std::move(levels_[level]);
            levels_[level].clear();
        }
    }
    
    if (!to_compact.empty()) {
        mergeSSTables(level + 1, to_compact);
        compaction_count_++;
    }
}

void CompactionLSMTree::mergeSSTables(
    size_t target_level,
    const std::vector<std::unique_ptr<lsm::naive::SSTable>>& sstables
) {
    // For now, just create new SSTables at the target level
    // In a real implementation, we would merge the data
    if (!sstables.empty()) {
        // Create a new temporary memtable to collect all data
        auto merged_memtable = std::make_unique<lsm::naive::MemTable>();
        
        // Collect all data from source SSTables
        for (const auto& sstable : sstables) {
            sstable->forEachEntry([&](const std::string& key, const std::string& value) {
                merged_memtable->put(key, value);
            });
        }
        
        // Create a new SSTable from the merged data
        if (merged_memtable->size() > 0) {
            auto new_sstable = lsm::naive::SSTable::createFromMemTable(data_dir_, *merged_memtable);
            levels_[target_level].push_back(std::move(new_sstable));
        }
    }
    
    // Update metrics - in a real implementation, this would track actual I/O
    size_t total_size = 0;
    for (const auto& sstable : sstables) {
        total_size += sstable->getSizeBytes();
    }
    updateMetrics(total_size, total_size);
}

bool CompactionLSMTree::shouldCompactLevel(size_t level) const {
    if (level == 0) return levels_[0].size() >= sstable_threshold_L0_;
    
    size_t current_size = 0;
    for (const auto& sstable : levels_[level]) {
        current_size += sstable->getSizeBytes();
    }
    
    return current_size > calculateTargetLevelSize(level);
}

size_t CompactionLSMTree::calculateTargetLevelSize(size_t level) const {
    if (level == 0) return 0;  // L0 size is controlled by SSTable count
    
    size_t base_size = 0;
    for (const auto& sstable : levels_[0]) {
        base_size += sstable->getSizeBytes();
    }
    
    return base_size * std::pow(size_ratio_, level);
}

void CompactionLSMTree::updateMetrics(size_t bytes_written, size_t bytes_read) {
    total_bytes_written_ += bytes_written;
    total_bytes_read_ += bytes_read;
}

std::string CompactionLSMTree::getStats() const {
    nlohmann::json stats;
    stats["sstable_threshold_L0"] = sstable_threshold_L0_;
    stats["size_ratio"] = size_ratio_;
    stats["compaction_policy"] = compaction_policy_;
    stats["total_operations"] = total_operations_;
    stats["compaction_count"] = compaction_count_;
    stats["total_bytes_written"] = total_bytes_written_;
    stats["total_bytes_read"] = total_bytes_read_;
    
    nlohmann::json level_stats;
    for (size_t i = 0; i < levels_.size(); ++i) {
        level_stats[std::to_string(i)] = {
            {"sstable_count", levels_[i].size()},
            {"total_size", getLevelSize(i)}
        };
    }
    stats["levels"] = level_stats;
    
    return stats.dump(2);
}

size_t CompactionLSMTree::getLevelSize(size_t level) const {
    if (level >= levels_.size()) return 0;
    
    size_t total_size = 0;
    for (const auto& sstable : levels_[level]) {
        total_size += sstable->getSizeBytes();
    }
    return total_size;
}

size_t CompactionLSMTree::getSSTableCount(size_t level) const {
    return level < levels_.size() ? levels_[level].size() : 0;
}

double CompactionLSMTree::getCompactionFrequency() const {
    return total_operations_ > 0 ? 
        static_cast<double>(compaction_count_) / total_operations_ : 0.0;
}

size_t CompactionLSMTree::getTotalBytesWritten() const {
    return total_bytes_written_;
}

size_t CompactionLSMTree::getTotalBytesRead() const {
    return total_bytes_read_;
}

} // namespace lsm_tree 