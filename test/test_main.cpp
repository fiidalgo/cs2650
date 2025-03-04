#include "common.h"
#include "skiplist.h"
#include "memtable.h"
#include "sstable.h"
#include "lsm_tree.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <filesystem>

using namespace lsm;

// Test SkipList
void testSkipList()
{
    std::cout << "Testing SkipList..." << std::endl;

    SkipList<Key, Value> skiplist;

    // Test insert and find
    skiplist.insert(1, 100);
    skiplist.insert(2, 200);
    skiplist.insert(3, 300);

    Value value;
    bool is_deleted;

    assert(skiplist.find(1, value, is_deleted));
    assert(value == 100);
    assert(!is_deleted);

    assert(skiplist.find(2, value, is_deleted));
    assert(value == 200);
    assert(!is_deleted);

    assert(skiplist.find(3, value, is_deleted));
    assert(value == 300);
    assert(!is_deleted);

    assert(!skiplist.find(4, value, is_deleted));

    // Test update
    skiplist.insert(2, 250);
    assert(skiplist.find(2, value, is_deleted));
    assert(value == 250);

    // Test delete
    skiplist.insert(2, 250, true);
    assert(skiplist.find(2, value, is_deleted));
    assert(is_deleted);

    // Test range query
    auto range_result = skiplist.range(1, 3);
    assert(range_result.size() == 2); // 1 and 3 (2 is deleted)
    assert(range_result[0].first == 1);
    assert(range_result[0].second == 100);
    assert(range_result[1].first == 3);
    assert(range_result[1].second == 300);

    std::cout << "SkipList tests passed!" << std::endl;
}

// Test MemTable
void testMemTable()
{
    std::cout << "Testing MemTable..." << std::endl;

    MemTable memtable;

    // Test put and get
    assert(memtable.put(1, 100) == Status::OK);
    assert(memtable.put(2, 200) == Status::OK);
    assert(memtable.put(3, 300) == Status::OK);

    Value value;
    assert(memtable.get(1, value) == Status::OK);
    assert(value == 100);

    assert(memtable.get(2, value) == Status::OK);
    assert(value == 200);

    assert(memtable.get(3, value) == Status::OK);
    assert(value == 300);

    assert(memtable.get(4, value) == Status::NOT_FOUND);

    // Test update
    assert(memtable.put(2, 250) == Status::OK);
    assert(memtable.get(2, value) == Status::OK);
    assert(value == 250);

    // Test delete
    assert(memtable.remove(2) == Status::OK);
    assert(memtable.get(2, value) == Status::NOT_FOUND);

    // Test range query
    std::vector<std::pair<Key, Value>> range_result;
    assert(memtable.range(1, 3, range_result) == Status::OK);
    assert(range_result.size() == 2); // 1 and 3 (2 is deleted)

    // Test flush
    std::string test_file = "test_memtable.sst";
    assert(memtable.flush(test_file) == Status::OK);

    // Clean up
    std::filesystem::remove(test_file);

    std::cout << "MemTable tests passed!" << std::endl;
}

// Test SSTable
void testSSTable()
{
    std::cout << "Testing SSTable..." << std::endl;

    // Create a MemTable and flush it to an SSTable
    MemTable memtable;
    memtable.put(1, 100);
    memtable.put(2, 200);
    memtable.put(3, 300);

    std::string test_file = "test_sstable.sst";
    assert(memtable.flush(test_file) == Status::OK);

    // Open the SSTable
    SSTable sstable(test_file);

    // Test get
    Value value;
    assert(sstable.get(1, value) == Status::OK);
    assert(value == 100);

    assert(sstable.get(2, value) == Status::OK);
    assert(value == 200);

    assert(sstable.get(3, value) == Status::OK);
    assert(value == 300);

    assert(sstable.get(4, value) == Status::NOT_FOUND);

    // Test range query
    std::vector<std::pair<Key, Value>> range_result;
    assert(sstable.range(1, 3, range_result) == Status::OK);
    assert(range_result.size() == 3);

    // Clean up
    std::filesystem::remove(test_file);

    std::cout << "SSTable tests passed!" << std::endl;
}

// Test LSM-Tree
void testLSMTree()
{
    std::cout << "Testing LSM-Tree..." << std::endl;

    // Create a test directory
    std::string test_dir = "test_lsm_tree";
    std::filesystem::create_directories(test_dir);

    // Create an LSM-Tree
    LSMTree lsm(test_dir);

    // Test put
    assert(lsm.put(1, 100) == Status::OK);
    assert(lsm.put(2, 200) == Status::OK);
    assert(lsm.put(3, 300) == Status::OK);

    Value value;
    assert(lsm.get(1, value) == Status::OK);
    assert(value == 100);

    assert(lsm.get(2, value) == Status::OK);
    assert(value == 200);

    assert(lsm.get(3, value) == Status::OK);
    assert(value == 300);

    assert(lsm.get(4, value) == Status::NOT_FOUND);

    // Test update
    assert(lsm.put(2, 250) == Status::OK);
    assert(lsm.get(2, value) == Status::OK);
    assert(value == 250);

    // Test delete
    assert(lsm.remove(2) == Status::OK);
    assert(lsm.get(2, value) == Status::NOT_FOUND);

    // Test range query
    std::vector<std::pair<Key, Value>> range_result;
    assert(lsm.range(1, 3, range_result) == Status::OK);
    assert(range_result.size() == 2); // 1 and 3 (2 is deleted)

    // Test direct flush instead of using background thread
    std::cout << "Testing direct flush..." << std::endl;

    // Create a new SSTable file
    std::string sstable_file = test_dir + "/direct_flush.sst";
    assert(lsm.getActiveMemTable()->flush(sstable_file) == Status::OK);

    // Create a new SSTable from the file and add it to level 0
    lsm.addSSTableToLevel0(sstable_file);

    // Test get after direct flush
    Status status = lsm.get(1, value);
    assert(status == Status::OK);
    assert(value == 100);

    status = lsm.get(3, value);
    assert(status == Status::OK);
    assert(value == 300);

    status = lsm.get(2, value);
    assert(status == Status::NOT_FOUND);

    // Clean up
    std::filesystem::remove_all(test_dir);

    std::cout << "LSM-Tree tests passed!" << std::endl;
}

int main()
{
    std::cout << "Running LSM-Tree tests..." << std::endl;

    testSkipList();
    testMemTable();
    testSSTable();
    testLSMTree();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}