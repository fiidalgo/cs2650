#include "naive/memtable.h"
#include <iostream>

namespace lsm {
namespace naive {

MemTable::MemTable() : size_bytes_(0) {}

bool MemTable::put(const std::string& key, const std::string& value) {
    // If key exists, update value and adjust size
    auto it = data_.find(key);
    if (it != data_.end()) {
        size_bytes_ -= it->first.size() + it->second.size();
        it->second = value;
        size_bytes_ += key.size() + value.size();
        return true;
    } else {
        // Insert new key-value pair
        data_[key] = value;
        size_bytes_ += key.size() + value.size();
        return false;
    }
}

std::optional<std::string> MemTable::get(const std::string& key) const {
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void MemTable::range(const std::string& start_key, const std::string& end_key,
                    const std::function<void(const std::string&, const std::string&)>& callback) const {
    auto it = data_.lower_bound(start_key);
    auto end = data_.upper_bound(end_key);
    
    while (it != end) {
        callback(it->first, it->second);
        ++it;
    }
}

size_t MemTable::size() const {
    return data_.size();
}

size_t MemTable::sizeBytes() const {
    return size_bytes_;
}

void MemTable::clear() {
    data_.clear();
    size_bytes_ = 0;
}

std::map<std::string, std::string>::const_iterator MemTable::begin() const {
    return data_.begin();
}

std::map<std::string, std::string>::const_iterator MemTable::end() const {
    return data_.end();
}

} // namespace naive
} // namespace lsm 