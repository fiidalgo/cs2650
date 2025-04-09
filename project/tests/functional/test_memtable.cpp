#include "naive/memtable.h"
#include <iostream>
#include <cassert>
#include <string>

// Helper function to print a value or "null" if it doesn't exist
void print_value(const std::optional<int32_t> &value)
{
    if (value.has_value())
    {
        std::cout << value.value();
    }
    else
    {
        std::cout << "null";
    }
}

// Helper function to print the result of a range query
void print_range(const std::vector<std::pair<int32_t, int32_t>> &range_result)
{
    std::cout << "Range result [" << range_result.size() << " entries]: ";
    for (const auto &[key, value] : range_result)
    {
        std::cout << key << ":" << value << " ";
    }
    std::cout << std::endl;
}

int main()
{
    std::cout << "Testing MemTable implementation..." << std::endl;

    // Create a new MemTable
    naive::MemTable memtable;

    // Test empty table properties
    assert(memtable.empty());
    assert(memtable.size() == 0);
    std::cout << "Empty table checks passed." << std::endl;

    // Test put and get operations
    memtable.put(1, 100);
    memtable.put(2, 200);
    memtable.put(3, 300);

    assert(!memtable.empty());
    assert(memtable.size() == 3);

    auto value1 = memtable.get(1);
    auto value2 = memtable.get(2);
    auto value3 = memtable.get(3);
    auto value4 = memtable.get(4); // Non-existent key

    assert(value1.has_value() && value1.value() == 100);
    assert(value2.has_value() && value2.value() == 200);
    assert(value3.has_value() && value3.value() == 300);
    assert(!value4.has_value());

    std::cout << "Put/get tests passed." << std::endl;

    // Test update operation
    memtable.put(2, 250); // Update existing key
    auto updated_value = memtable.get(2);
    assert(updated_value.has_value() && updated_value.value() == 250);
    std::cout << "Update test passed." << std::endl;

    // Test range queries
    auto range1 = memtable.range(1, 3);  // Should include keys 1, 2
    auto range2 = memtable.range(2, 5);  // Should include keys 2, 3
    auto range3 = memtable.range(5, 10); // Should be empty

    assert(range1.size() == 2);
    assert(range2.size() == 2);
    assert(range3.size() == 0);

    // Verify range contents
    assert(range1[0].first == 1 && range1[0].second == 100);
    assert(range1[1].first == 2 && range1[1].second == 250);

    std::cout << "Range query tests passed." << std::endl;

    // Test removal (tombstones)
    bool removed1 = memtable.remove(2);
    bool removed2 = memtable.remove(4); // Non-existent key

    assert(removed1);  // Should return true for existing key
    assert(!removed2); // Should return false for non-existent key

    // Size should still include the tombstone
    assert(memtable.size() == 3);

    // The removed key should return nullopt
    auto removed_value = memtable.get(2);
    assert(!removed_value.has_value());

    // Range query should exclude tombstones
    auto range_after_removal = memtable.range(1, 4);
    assert(range_after_removal.size() == 2); // Only keys 1 and 3 should be included

    std::cout << "Removal test passed." << std::endl;

    // Test for_each
    std::cout << "Testing for_each with all entries:" << std::endl;
    memtable.for_each([](int32_t key, const std::optional<int32_t> &value)
                      {
        std::cout << "Key: " << key << ", Value: ";
        print_value(value);
        std::cout << std::endl; });

    // Print a sample range query
    auto sample_range = memtable.range(1, 10);
    print_range(sample_range);

    std::cout << "All MemTable tests passed!" << std::endl;
    return 0;
}