#include "naive/lsm_tree.h"
#include "naive/memtable.h"
#include "naive/sstable.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lsm {
namespace naive {

LSMTree::LSMTree(std::string data_dir, size_t memtable_size_bytes)
    : data_dir_(std::move(data_dir)), 
      memtable_size_bytes_(memtable_size_bytes),
      memtable_(std::make_unique<MemTable>()) {
    
    // Create data directory if it doesn't exist
    if (!fs::exists(data_dir_)) {
        fs::create_directories(data_dir_);
    }
    
    // Load existing SSTables
    loadExistingSSTables();
}

LSMTree::~LSMTree() {
    close();
}

void LSMTree::loadExistingSSTables() {
    if (!fs::exists(data_dir_)) {
        return;
    }
    
    std::regex sstable_regex("sstable_([0-9]+)\\.sst");
    
    for (const auto& entry : fs::directory_iterator(data_dir_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        
        std::string filename = entry.path().filename().string();
        std::smatch match;
        
        if (std::regex_match(filename, match, sstable_regex)) {
            // Load SSTable
            auto sstable = SSTable::load(entry.path().string());
            if (sstable) {
                sstables_.push_back(std::move(sstable));
            }
        }
    }
    
    // Sort SSTables by timestamp (newest first)
    std::sort(sstables_.begin(), sstables_.end(), 
             [](const auto& a, const auto& b) {
                 return a->getTimestamp() > b->getTimestamp();
             });
}

void LSMTree::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if we need to flush the MemTable
    if (memtable_->sizeBytes() + key.size() + value.size() > memtable_size_bytes_) {
        flushMemTable();
    }
    
    // Insert into MemTable
    memtable_->put(key, value);
}

std::optional<std::string> LSMTree::get(const std::string& key, GetMetadata* metadata) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // First, check the memtable
    auto value = memtable_->get(key);
    if (value) {
        // Check if it's a tombstone
        if (*value == "__TOMBSTONE__") {
            return std::nullopt;
        }
        return value;
    }
    
    // Then, check SSTables in order from newest to oldest
    for (const auto& sstable : sstables_) {
        value = sstable->get(key, metadata);
        if (value) {
            // Check if it's a tombstone
            if (*value == "__TOMBSTONE__") {
                return std::nullopt;
            }
            return value;
        }
    }
    
    return std::nullopt;
}

void LSMTree::range(const std::string& start_key, const std::string& end_key,
                   const std::function<void(const std::string&, const std::string&)>& callback) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Collect all key-value pairs in the range
    std::map<std::string, std::string> results;
    std::set<std::string> tombstones;
    
    // First, collect from memtable
    memtable_->range(start_key, end_key, [&](const std::string& key, const std::string& value) {
        if (value == "__TOMBSTONE__") {
            tombstones.insert(key);
        } else {
            results[key] = value;
        }
    });
    
    // Then, collect from SSTables
    for (const auto& sstable : sstables_) {
        sstable->range(start_key, end_key, [&](const std::string& key, const std::string& value) {
            // Skip tombstones and already collected keys
            if (tombstones.find(key) != tombstones.end() || results.find(key) != results.end()) {
                return;
            }
            
            if (value == "__TOMBSTONE__") {
                tombstones.insert(key);
            } else {
                results[key] = value;
            }
        });
    }
    
    // Call the callback for each result
    for (const auto& [key, value] : results) {
        callback(key, value);
    }
}

void LSMTree::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Insert a tombstone
    memtable_->put(key, "__TOMBSTONE__");
    
    // Check if we need to flush the MemTable
    if (memtable_->sizeBytes() > memtable_size_bytes_) {
        flushMemTable();
    }
}

void LSMTree::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    flushMemTable();
}

void LSMTree::flushMemTable() {
    // Skip if the MemTable is empty
    if (memtable_->size() == 0) {
        return;
    }
    
    // Create an SSTable from the MemTable
    auto sstable = SSTable::createFromMemTable(data_dir_, *memtable_);
    
    // Add to the list of SSTables
    sstables_.insert(sstables_.begin(), std::move(sstable));
    
    // Clear the MemTable
    memtable_->clear();
}

void LSMTree::compact() {
    // Not implemented in the naive version
    return;
}

void LSMTree::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Flush any pending data
    flushMemTable();
}

std::string LSMTree::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["memtable_size_bytes"] = memtable_->sizeBytes();
    stats["memtable_entries"] = memtable_->size();
    stats["sstable_count"] = sstables_.size();
    
    size_t total_entries = memtable_->size();
    size_t total_size_bytes = memtable_->sizeBytes();
    
    json sstables = json::array();
    for (const auto& sstable : sstables_) {
        json sstable_stats;
        sstable_stats["timestamp"] = sstable->getTimestamp();
        sstable_stats["entries"] = sstable->getCount();
        sstable_stats["size_bytes"] = sstable->getSizeBytes();
        sstable_stats["min_key"] = sstable->getMinKey();
        sstable_stats["max_key"] = sstable->getMaxKey();
        
        total_entries += sstable->getCount();
        total_size_bytes += sstable->getSizeBytes();
        
        sstables.push_back(sstable_stats);
    }
    
    stats["sstables"] = sstables;
    stats["total_entries"] = total_entries;
    stats["total_size_bytes"] = total_size_bytes;
    
    return stats.dump(2);
}

size_t LSMTree::getSStableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sstables_.size();
}

size_t LSMTree::getMemTableSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return memtable_->sizeBytes();
}

size_t LSMTree::getTotalSizeBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t total_size = memtable_->sizeBytes();
    for (const auto& sstable : sstables_) {
        total_size += sstable->getSizeBytes();
    }
    
    return total_size;
}

void LSMTree::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clear the MemTable
    memtable_->clear();
    
    // Clear the SSTables
    for (const auto& sstable : sstables_) {
        sstable->remove();
    }
    sstables_.clear();
}

} // namespace naive
} // namespace lsm 