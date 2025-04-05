#include "common.h"
#include "lsm_tree.h"
#include <iostream>
#include <chrono>
#include <random>
#include <filesystem>
#include <fstream>
#include <cassert>

using namespace lsm;

// Test compaction with a large number of sequential inserts
void testSequentialCompaction()
{
    std::cout << "Testing sequential compaction..." << std::endl;

    // Create a test directory
    std::string test_dir = "test_sequential_compaction";
    std::filesystem::create_directories(test_dir);

    // Create an LSM-Tree
    LSMTree lsm(test_dir);

    // Track initial I/O stats
    size_t initial_reads = IOTracker::getInstance().getReadCount();
    size_t initial_writes = IOTracker::getInstance().getWriteCount();

    // Insert enough key-value pairs to trigger multiple compactions
    // This should create at least 5 levels
    const int NUM_INSERTS = 50000;
    std::cout << "Inserting " << NUM_INSERTS << " key-value pairs..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_INSERTS; i++)
    {
        Key key = i;
        Value value = i * 10;
        Status status = lsm.put(key, value);
        assert(status == Status::OK);

        // Periodically flush to trigger compaction
        if (i % 1000 == 0 && i > 0)
        {
            lsm.flushAllMemTables();
            std::cout << "Inserted " << i << " key-value pairs..." << std::endl;
        }
    }

    // Final flush to ensure all data is on disk
    lsm.flushAllMemTables();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Get stats after insertion
    std::map<std::string, std::string> stats;
    lsm.getStats(stats);

    // Calculate I/O stats
    size_t total_reads = IOTracker::getInstance().getReadCount() - initial_reads;
    size_t total_writes = IOTracker::getInstance().getWriteCount() - initial_writes;
    size_t total_read_bytes = IOTracker::getInstance().getReadBytes();
    size_t total_write_bytes = IOTracker::getInstance().getWriteBytes();

    // Print results
    std::cout << "Sequential insertion completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Total I/O: " << total_reads << " reads, " << total_writes << " writes" << std::endl;
    std::cout << "Total I/O bytes: " << total_read_bytes << " read, " << total_write_bytes << " written" << std::endl;

    std::cout << "LSM-Tree Statistics:" << std::endl;
    for (const auto &stat : stats)
    {
        std::cout << "  " << stat.first << ": " << stat.second << std::endl;
    }

    // Verify the data by reading it back
    std::cout << "Verifying data..." << std::endl;

    int verify_count = 0;
    for (int i = 0; i < NUM_INSERTS; i += 1000) // Check every 1000th key to speed up the test
    {
        Key key = i;
        Value value;
        Status status = lsm.get(key, value);

        if (status == Status::OK)
        {
            assert(value == i * 10);
            verify_count++;
        }
        else
        {
            std::cerr << "Error: Key " << key << " not found!" << std::endl;
        }
    }

    std::cout << "Successfully verified " << verify_count << " keys" << std::endl;

    // Clean up
    std::filesystem::remove_all(test_dir);

    std::cout << "Sequential compaction test completed!" << std::endl
              << std::endl;
}

// Test compaction with random inserts
void testRandomCompaction()
{
    std::cout << "Testing random compaction..." << std::endl;

    // Create a test directory
    std::string test_dir = "test_random_compaction";
    std::filesystem::create_directories(test_dir);

    // Create an LSM-Tree
    LSMTree lsm(test_dir);

    // Track initial I/O stats
    size_t initial_reads = IOTracker::getInstance().getReadCount();
    size_t initial_writes = IOTracker::getInstance().getWriteCount();

    // Random number generator
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<Key> key_dist(1, 1000000);

    // Insert random key-value pairs
    const int NUM_INSERTS = 50000;
    std::cout << "Inserting " << NUM_INSERTS << " random key-value pairs..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Keep track of inserted keys for verification
    std::vector<Key> inserted_keys;

    for (int i = 0; i < NUM_INSERTS; i++)
    {
        Key key = key_dist(rng);
        Value value = i;
        Status status = lsm.put(key, value);
        assert(status == Status::OK);

        inserted_keys.push_back(key);

        // Periodically flush to trigger compaction
        if (i % 1000 == 0 && i > 0)
        {
            lsm.flushAllMemTables();
            std::cout << "Inserted " << i << " key-value pairs..." << std::endl;
        }
    }

    // Final flush to ensure all data is on disk
    lsm.flushAllMemTables();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Get stats after insertion
    std::map<std::string, std::string> stats;
    lsm.getStats(stats);

    // Calculate I/O stats
    size_t total_reads = IOTracker::getInstance().getReadCount() - initial_reads;
    size_t total_writes = IOTracker::getInstance().getWriteCount() - initial_writes;
    size_t total_read_bytes = IOTracker::getInstance().getReadBytes();
    size_t total_write_bytes = IOTracker::getInstance().getWriteBytes();

    // Print results
    std::cout << "Random insertion completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Total I/O: " << total_reads << " reads, " << total_writes << " writes" << std::endl;
    std::cout << "Total I/O bytes: " << total_read_bytes << " read, " << total_write_bytes << " written" << std::endl;

    std::cout << "LSM-Tree Statistics:" << std::endl;
    for (const auto &stat : stats)
    {
        std::cout << "  " << stat.first << ": " << stat.second << std::endl;
    }

    // Verify some of the data by reading it back
    std::cout << "Verifying data..." << std::endl;

    int verify_count = 0;
    int verify_sample_size = std::min(1000, (int)inserted_keys.size());

    for (int i = 0; i < verify_sample_size; i++)
    {
        int idx = (i * inserted_keys.size()) / verify_sample_size;
        Key key = inserted_keys[idx];
        Value value;
        Status status = lsm.get(key, value);

        if (status == Status::OK)
        {
            verify_count++;
        }
        else
        {
            std::cerr << "Error: Key " << key << " not found!" << std::endl;
        }
    }

    std::cout << "Successfully verified " << verify_count << " of " << verify_sample_size << " keys" << std::endl;

    // Clean up
    std::filesystem::remove_all(test_dir);

    std::cout << "Random compaction test completed!" << std::endl
              << std::endl;
}

// Test manual compaction command
void testManualCompaction()
{
    std::cout << "Testing manual compaction..." << std::endl;

    // Create a test directory
    std::string test_dir = "test_manual_compaction";
    std::filesystem::create_directories(test_dir);

    // Create an LSM-Tree
    LSMTree lsm(test_dir);

    // Insert enough key-value pairs to potentially trigger compaction
    const int NUM_INSERTS = 10000;
    std::cout << "Inserting " << NUM_INSERTS << " key-value pairs..." << std::endl;

    for (int i = 0; i < NUM_INSERTS; i++)
    {
        Key key = i;
        Value value = i * 10;
        Status status = lsm.put(key, value);
        assert(status == Status::OK);
    }

    // Flush to create SSTables but don't wait for automatic compaction
    lsm.flushAllMemTables();

    // Get stats before manual compaction
    std::map<std::string, std::string> before_stats;
    lsm.getStats(before_stats);

    std::cout << "Before manual compaction:" << std::endl;
    for (const auto &stat : before_stats)
    {
        std::cout << "  " << stat.first << ": " << stat.second << std::endl;
    }

    // Track I/O before manual compaction
    size_t before_reads = IOTracker::getInstance().getReadCount();
    size_t before_writes = IOTracker::getInstance().getWriteCount();

    // Trigger manual compaction
    std::cout << "Triggering manual compaction..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    Status compact_status = lsm.compact();
    assert(compact_status == Status::OK);

    // Wait a bit for the compaction to finish (since it runs in background)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Get stats after manual compaction
    std::map<std::string, std::string> after_stats;
    lsm.getStats(after_stats);

    // Calculate I/O for compaction
    size_t compaction_reads = IOTracker::getInstance().getReadCount() - before_reads;
    size_t compaction_writes = IOTracker::getInstance().getWriteCount() - before_writes;

    // Print results
    std::cout << "Manual compaction completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Compaction I/O: " << compaction_reads << " reads, " << compaction_writes << " writes" << std::endl;

    std::cout << "After manual compaction:" << std::endl;
    for (const auto &stat : after_stats)
    {
        std::cout << "  " << stat.first << ": " << stat.second << std::endl;
    }

    // Verify the data is still accessible
    std::cout << "Verifying data after compaction..." << std::endl;

    int verify_count = 0;
    for (int i = 0; i < NUM_INSERTS; i += 500) // Check every 500th key
    {
        Key key = i;
        Value value;
        Status status = lsm.get(key, value);

        if (status == Status::OK)
        {
            assert(value == i * 10);
            verify_count++;
        }
        else
        {
            std::cerr << "Error: Key " << key << " not found after compaction!" << std::endl;
        }
    }

    std::cout << "Successfully verified " << verify_count << " keys after compaction" << std::endl;

    // Clean up
    std::filesystem::remove_all(test_dir);

    std::cout << "Manual compaction test completed!" << std::endl;
}

int main()
{
    std::cout << "Running LSM-Tree compaction tests..." << std::endl;

    // Reset IO tracking
    IOTracker::getInstance().reset();

    testSequentialCompaction();
    testRandomCompaction();
    testManualCompaction();

    std::cout << "All compaction tests passed!" << std::endl;
    return 0;
}