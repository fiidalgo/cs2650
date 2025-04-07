#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include "naive/lsm_tree.h"

namespace fs = std::filesystem;

// Print a divider for readability
void printDivider() {
    std::cout << "\n" << std::string(50, '-') << "\n" << std::endl;
}

int main() {
    // Test directory
    std::string data_dir = "project/data/test_cpp";
    
    // Clean up any existing data
    if (fs::exists(data_dir)) {
        fs::remove_all(data_dir);
    }
    fs::create_directories(data_dir);
    
    // Create a small LSM-Tree for testing with a small MemTable size to force flushes
    std::cout << "Creating LSM-Tree with small MemTable size (200 bytes)" << std::endl;
    lsm::naive::LSMTree lsm(data_dir, 200);
    
    // Insert some data
    std::cout << "Inserting data..." << std::endl;
    lsm.put("apple", "red");
    lsm.put("banana", "yellow");
    std::cout << "MemTable size: " << lsm.getMemTableSize() << " bytes" << std::endl;
    
    // Insert more data to trigger a flush
    lsm.put("cherry", "red");
    lsm.put("date", "brown");
    lsm.put("elderberry", "purple");
    std::cout << "MemTable size after more inserts: " << lsm.getMemTableSize() << " bytes" << std::endl;
    std::cout << "SSTable count: " << lsm.getSStableCount() << std::endl;
    
    printDivider();
    
    // Retrieve some values
    auto apple_value = lsm.get("apple");
    std::cout << "Value for 'apple': " << (apple_value ? *apple_value : "None") << std::endl;
    
    auto fig_value = lsm.get("fig");
    std::cout << "Value for 'fig' (nonexistent): " << (fig_value ? *fig_value : "None") << std::endl;
    
    // Test getting with metadata
    lsm::naive::GetMetadata metadata;
    auto banana_value = lsm.get("banana", &metadata);
    std::cout << "Value for 'banana': " << (banana_value ? *banana_value : "None") << std::endl;
    std::cout << "SSTables accessed: " << metadata.sstables_accessed << std::endl;
    std::cout << "Bytes read: " << metadata.bytes_read << std::endl;
    
    printDivider();
    
    // Perform a range query
    std::cout << "Range query from 'banana' to 'elderberry':" << std::endl;
    lsm.range("banana", "elderberry", [](const std::string& key, const std::string& value) {
        std::cout << "  " << key << ": " << value << std::endl;
    });
    
    printDivider();
    
    // Delete a key
    std::cout << "Deleting key 'cherry'" << std::endl;
    lsm.remove("cherry");
    auto cherry_value = lsm.get("cherry");
    std::cout << "Value for 'cherry' after deletion: " << (cherry_value ? *cherry_value : "None") << std::endl;
    
    printDivider();
    
    // Get statistics
    std::cout << "LSM-Tree stats:" << std::endl;
    std::cout << lsm.getStats() << std::endl;
    
    printDivider();
    
    // Test flush
    std::cout << "Manually flushing MemTable" << std::endl;
    lsm.flush();
    std::cout << "MemTable size after flush: " << lsm.getMemTableSize() << " bytes" << std::endl;
    std::cout << "SSTable count: " << lsm.getSStableCount() << std::endl;
    
    printDivider();
    
    // Close the LSM-Tree
    std::cout << "Closing LSM-Tree" << std::endl;
    lsm.close();
    
    // Test reopening the LSM-Tree
    std::cout << "Reopening LSM-Tree" << std::endl;
    lsm::naive::LSMTree lsm2(data_dir, 200);
    
    // Verify data is still accessible
    auto banana_value2 = lsm2.get("banana");
    std::cout << "Value for 'banana' after reopen: " << (banana_value2 ? *banana_value2 : "None") << std::endl;
    
    printDivider();
    
    // Clean up
    std::cout << "Cleaning up" << std::endl;
    lsm2.clear();
    std::cout << "SSTable count after cleanup: " << lsm2.getSStableCount() << std::endl;
    
    std::cout << "\nAll tests completed successfully!" << std::endl;
    
    return 0;
} 