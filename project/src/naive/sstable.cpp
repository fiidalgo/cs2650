#include "naive/sstable.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include "naive/lsm_tree.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lsm {
namespace naive {

SSTable::SSTable(std::string file_path, uint64_t timestamp)
    : file_path_(std::move(file_path)), timestamp_(timestamp), count_(0), min_key_(""), max_key_(""), header_size_(0) {
    
    if (timestamp_ == 0) {
        // Generate timestamp if not provided
        auto now = std::chrono::system_clock::now();
        timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }
}

// Static method to create an SSTable from a MemTable
std::unique_ptr<SSTable> SSTable::createFromMemTable(const std::string& data_dir, 
                                                  const MemTable& memtable) {
    // Create data directory if it doesn't exist
    if (!fs::exists(data_dir)) {
        fs::create_directories(data_dir);
    }
    
    // Generate a unique filename based on the timestamp
    auto now = std::chrono::system_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::string filename = "sstable_" + std::to_string(timestamp) + ".sst";
    std::string file_path = data_dir + "/" + filename;
    
    // Create an SSTable instance
    auto sstable = std::make_unique<SSTable>(file_path, timestamp);
    
    // Extract the data from memtable
    std::map<std::string, std::string> entries;
    for (auto it = memtable.begin(); it != memtable.end(); ++it) {
        entries[it->first] = it->second;
    }
    
    // Write the data to the SSTable
    sstable->writeSSTable(entries);
    
    return sstable;
}

void SSTable::writeSSTable(const std::map<std::string, std::string>& entries) {
    if (entries.empty()) {
        return; // Nothing to write
    }
    
    std::ofstream file(file_path_, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + file_path_);
    }
    
    // Prepare header data
    count_ = entries.size();
    min_key_ = entries.begin()->first;
    max_key_ = (--entries.end())->first;
    
    // Write entries first to calculate total size
    std::stringstream entries_stream;
    for (const auto& [key, value] : entries) {
        // Format: key_size(4 bytes) + key + value_size(4 bytes) + value
        uint32_t key_size = key.size();
        uint32_t value_size = value.size();
        
        entries_stream.write(reinterpret_cast<const char*>(&key_size), sizeof(key_size));
        entries_stream.write(key.c_str(), key_size);
        entries_stream.write(reinterpret_cast<const char*>(&value_size), sizeof(value_size));
        entries_stream.write(value.c_str(), value_size);
    }
    
    // Create JSON header
    json header;
    header["timestamp"] = timestamp_;
    header["count"] = count_;
    header["min_key"] = min_key_;
    header["max_key"] = max_key_;
    std::string header_str = header.dump();
    
    // Write header size (4 bytes)
    uint32_t header_size = header_str.size();
    header_size_ = header_size;
    file.write(reinterpret_cast<const char*>(&header_size), sizeof(header_size));
    
    // Write header
    file.write(header_str.c_str(), header_size);
    
    // Write entries
    file << entries_stream.rdbuf();
    
    file.close();
}

std::unique_ptr<SSTable> SSTable::load(const std::string& file_path) {
    auto sstable = std::make_unique<SSTable>(file_path);
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return nullptr;
    }
    
    // Read header size
    uint32_t header_size;
    file.read(reinterpret_cast<char*>(&header_size), sizeof(header_size));
    sstable->header_size_ = header_size;
    
    // Read header
    std::string header_str(header_size, ' ');
    file.read(&header_str[0], header_size);
    
    try {
        json header = json::parse(header_str);
        sstable->timestamp_ = header["timestamp"];
        sstable->count_ = header["count"];
        sstable->min_key_ = header["min_key"];
        sstable->max_key_ = header["max_key"];
    } catch (const std::exception& e) {
        std::cerr << "Error parsing header: " << e.what() << std::endl;
        return nullptr;
    }
    
    file.close();
    return sstable;
}

std::optional<std::string> SSTable::get(const std::string& key, void* metadata) const {
    // Skip if key is outside the range of this SSTable
    if (key < min_key_ || key > max_key_) {
        return std::nullopt;
    }
    
    std::ifstream file(file_path_, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    GetMetadata* meta = nullptr;
    if (metadata) {
        meta = static_cast<GetMetadata*>(metadata);
        meta->sstables_accessed++;
    }
    
    // Skip header
    file.seekg(sizeof(uint32_t) + header_size_);
    
    // Read entries
    size_t bytes_read = 0;
    while (file.good() && !file.eof()) {
        // Read key size
        uint32_t key_size;
        file.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        if (file.eof()) break;
        bytes_read += sizeof(key_size);
        
        // Read key
        std::string current_key(key_size, ' ');
        file.read(&current_key[0], key_size);
        bytes_read += key_size;
        
        // Read value size
        uint32_t value_size;
        file.read(reinterpret_cast<char*>(&value_size), sizeof(value_size));
        bytes_read += sizeof(value_size);
        
        if (current_key == key) {
            // Found the key, read the value
            std::string value(value_size, ' ');
            file.read(&value[0], value_size);
            bytes_read += value_size;
            
            if (meta) {
                meta->bytes_read += bytes_read;
            }
            
            file.close();
            return value;
        } else {
            // Skip this value
            file.seekg(value_size, std::ios::cur);
            bytes_read += value_size;
        }
    }
    
    if (meta) {
        meta->bytes_read += bytes_read;
    }
    
    file.close();
    return std::nullopt;
}

void SSTable::range(const std::string& start_key, const std::string& end_key,
                   const std::function<void(const std::string&, const std::string&)>& callback) const {
    // Skip if there's no overlap with the SSTable's range
    if (end_key < min_key_ || start_key > max_key_) {
        return;
    }
    
    std::ifstream file(file_path_, std::ios::binary);
    if (!file.is_open()) {
        return;
    }
    
    // Skip header
    file.seekg(sizeof(uint32_t) + header_size_);
    
    // Read entries
    while (file.good() && !file.eof()) {
        // Read key size
        uint32_t key_size;
        file.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        if (file.eof()) break;
        
        // Read key
        std::string key(key_size, ' ');
        file.read(&key[0], key_size);
        
        // Read value size
        uint32_t value_size;
        file.read(reinterpret_cast<char*>(&value_size), sizeof(value_size));
        
        // Check if key is in range
        if (key >= start_key && key <= end_key) {
            // Read value
            std::string value(value_size, ' ');
            file.read(&value[0], value_size);
            
            // Call the callback
            callback(key, value);
        } else if (key > end_key) {
            // We've passed the end of the range, stop
            break;
        } else {
            // Skip this value
            file.seekg(value_size, std::ios::cur);
        }
    }
    
    file.close();
}

void SSTable::forEachEntry(const std::function<void(const std::string&, const std::string&)>& callback) const {
    std::ifstream file(file_path_, std::ios::binary);
    if (!file.is_open()) {
        return;
    }
    
    // Skip header
    file.seekg(sizeof(uint32_t) + header_size_);
    
    // Read entries
    while (file.good() && !file.eof()) {
        // Read key size
        uint32_t key_size;
        file.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        if (file.eof()) break;
        
        // Read key
        std::string key(key_size, ' ');
        file.read(&key[0], key_size);
        
        // Read value size
        uint32_t value_size;
        file.read(reinterpret_cast<char*>(&value_size), sizeof(value_size));
        
        // Read value
        std::string value(value_size, ' ');
        file.read(&value[0], value_size);
        
        // Call the callback
        callback(key, value);
    }
    
    file.close();
}

uint64_t SSTable::getTimestamp() const {
    return timestamp_;
}

size_t SSTable::getCount() const {
    return count_;
}

const std::string& SSTable::getMinKey() const {
    return min_key_;
}

const std::string& SSTable::getMaxKey() const {
    return max_key_;
}

size_t SSTable::getSizeBytes() const {
    return fs::file_size(file_path_);
}

void SSTable::remove() const {
    fs::remove(file_path_);
}

} // namespace naive
} // namespace lsm