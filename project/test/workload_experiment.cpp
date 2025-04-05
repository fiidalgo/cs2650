#include "common.h"
#include "lsm_tree.h"
#include <iostream>
#include <chrono>
#include <random>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cassert>

using namespace lsm;

// Workload types
enum class WorkloadType
{
    READ_HEAVY,  // 80% reads, 20% writes
    WRITE_HEAVY, // 20% reads, 80% writes
    BALANCED,    // 50% reads, 50% writes
    SCAN_HEAVY   // 40% reads, 20% writes, 40% scans
};

// Result structure for experiment
struct ExperimentResult
{
    WorkloadType type;
    size_t operation_count;
    double total_time_ms;
    double avg_read_time_ms;
    double avg_write_time_ms;
    double avg_scan_time_ms;
    size_t read_count;
    size_t write_count;
    size_t scan_count;
    size_t compaction_count;
    size_t total_io_reads;
    size_t total_io_writes;
    size_t final_sstable_count;
};

// Run a specific workload experiment
ExperimentResult runWorkloadExperiment(WorkloadType type, size_t operation_count)
{
    ExperimentResult result;
    result.type = type;
    result.operation_count = operation_count;
    result.read_count = 0;
    result.write_count = 0;
    result.scan_count = 0;

    // Initialize result times
    result.total_time_ms = 0;
    result.avg_read_time_ms = 0;
    result.avg_write_time_ms = 0;
    result.avg_scan_time_ms = 0;

    // Create a test directory
    std::string test_dir = "workload_experiment_" + std::to_string(static_cast<int>(type));
    std::filesystem::create_directories(test_dir);

    // Create an LSM-Tree
    LSMTree lsm(test_dir);

    // Setup random number generator
    std::mt19937_64 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<Key> key_dist(1, 1000000);
    std::uniform_int_distribution<int> value_dist(1, 1000000);

    // Workload distribution based on type
    int read_pct, write_pct, scan_pct;

    switch (type)
    {
    case WorkloadType::READ_HEAVY:
        read_pct = 80;
        write_pct = 20;
        scan_pct = 0;
        std::cout << "Running READ_HEAVY workload (80% reads, 20% writes)" << std::endl;
        break;
    case WorkloadType::WRITE_HEAVY:
        read_pct = 20;
        write_pct = 80;
        scan_pct = 0;
        std::cout << "Running WRITE_HEAVY workload (20% reads, 80% writes)" << std::endl;
        break;
    case WorkloadType::BALANCED:
        read_pct = 50;
        write_pct = 50;
        scan_pct = 0;
        std::cout << "Running BALANCED workload (50% reads, 50% writes)" << std::endl;
        break;
    case WorkloadType::SCAN_HEAVY:
        read_pct = 40;
        write_pct = 20;
        scan_pct = 40;
        std::cout << "Running SCAN_HEAVY workload (40% reads, 20% writes, 40% scans)" << std::endl;
        break;
    }

    // Pre-load some data to have something to read
    std::cout << "Pre-loading initial data..." << std::endl;
    const int PRELOAD_COUNT = 10000;
    std::vector<Key> keys_in_db;

    for (int i = 0; i < PRELOAD_COUNT; i++)
    {
        Key key = key_dist(rng);
        Value value = value_dist(rng);
        lsm.put(key, value);
        keys_in_db.push_back(key);
    }

    // Flush to ensure data is on disk
    lsm.flushAllMemTables();

    // Reset I/O tracking before actual test
    IOTracker::getInstance().reset();

    // Track time for various operations
    double total_read_time_ms = 0;
    double total_write_time_ms = 0;
    double total_scan_time_ms = 0;

    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "Running " << operation_count << " operations..." << std::endl;

    // Execute operations according to workload distribution
    for (size_t i = 0; i < operation_count; i++)
    {
        // Determine operation type
        int op_rand = std::uniform_int_distribution<int>(1, 100)(rng);

        if (op_rand <= read_pct)
        {
            // Read operation
            Key key;
            if (!keys_in_db.empty())
            {
                // Read an existing key
                key = keys_in_db[std::uniform_int_distribution<size_t>(0, keys_in_db.size() - 1)(rng)];
            }
            else
            {
                // Generate a random key if none exist yet
                key = key_dist(rng);
            }

            auto read_start = std::chrono::high_resolution_clock::now();

            Value value;
            lsm.get(key, value);

            auto read_end = std::chrono::high_resolution_clock::now();
            auto read_duration = std::chrono::duration_cast<std::chrono::microseconds>(read_end - read_start);

            total_read_time_ms += read_duration.count() / 1000.0;
            result.read_count++;
        }
        else if (op_rand <= read_pct + write_pct)
        {
            // Write operation
            Key key = key_dist(rng);
            Value value = value_dist(rng);

            auto write_start = std::chrono::high_resolution_clock::now();

            lsm.put(key, value);

            auto write_end = std::chrono::high_resolution_clock::now();
            auto write_duration = std::chrono::duration_cast<std::chrono::microseconds>(write_end - write_start);

            total_write_time_ms += write_duration.count() / 1000.0;
            result.write_count++;

            // Add to known keys
            keys_in_db.push_back(key);

            // Keep keys_in_db from growing too large
            if (keys_in_db.size() > 100000)
            {
                keys_in_db.erase(keys_in_db.begin(), keys_in_db.begin() + 10000);
            }
        }
        else
        {
            // Scan operation
            if (!keys_in_db.empty())
            {
                // Pick two random keys for range
                int idx1 = std::uniform_int_distribution<size_t>(0, keys_in_db.size() - 1)(rng);
                int idx2 = std::uniform_int_distribution<size_t>(0, keys_in_db.size() - 1)(rng);

                Key start_key = keys_in_db[std::min(idx1, idx2)];
                Key end_key = keys_in_db[std::max(idx1, idx2)];

                auto scan_start = std::chrono::high_resolution_clock::now();

                std::vector<std::pair<Key, Value>> results;
                lsm.range(start_key, end_key, results);

                auto scan_end = std::chrono::high_resolution_clock::now();
                auto scan_duration = std::chrono::duration_cast<std::chrono::microseconds>(scan_end - scan_start);

                total_scan_time_ms += scan_duration.count() / 1000.0;
                result.scan_count++;
            }
        }

        // Periodically report progress
        if (i % 1000 == 0 && i > 0)
        {
            std::cout << "  Completed " << i << " operations..." << std::endl;
        }
    }

    // Final flush to ensure all data is persisted
    lsm.flushAllMemTables();

    // End timing
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Calculate results
    result.total_time_ms = duration.count();

    if (result.read_count > 0)
        result.avg_read_time_ms = total_read_time_ms / result.read_count;

    if (result.write_count > 0)
        result.avg_write_time_ms = total_write_time_ms / result.write_count;

    if (result.scan_count > 0)
        result.avg_scan_time_ms = total_scan_time_ms / result.scan_count;

    // Get I/O statistics
    result.total_io_reads = IOTracker::getInstance().getReadCount();
    result.total_io_writes = IOTracker::getInstance().getWriteCount();

    // Get LSM-Tree statistics
    std::map<std::string, std::string> stats;
    lsm.getStats(stats);

    // Extract relevant stats
    result.compaction_count = std::stoi(stats["compactions_performed"]);
    result.final_sstable_count = std::stoi(stats["total_sstables"]);

    // Clean up
    std::filesystem::remove_all(test_dir);

    return result;
}

// Print experiment results
void printResults(const std::vector<ExperimentResult> &results)
{
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n========== WORKLOAD EXPERIMENT RESULTS ==========\n"
              << std::endl;

    std::cout << std::setw(15) << "Workload Type"
              << std::setw(10) << "Ops"
              << std::setw(10) << "Time(ms)"
              << std::setw(10) << "Rd Time"
              << std::setw(10) << "Wr Time"
              << std::setw(10) << "Scan Time"
              << std::setw(10) << "Compacts"
              << std::setw(10) << "I/O Rd"
              << std::setw(10) << "I/O Wr"
              << std::setw(10) << "SSTables"
              << std::endl;

    std::cout << std::string(105, '-') << std::endl;

    for (const auto &result : results)
    {
        std::string workload_name;
        switch (result.type)
        {
        case WorkloadType::READ_HEAVY:
            workload_name = "READ_HEAVY";
            break;
        case WorkloadType::WRITE_HEAVY:
            workload_name = "WRITE_HEAVY";
            break;
        case WorkloadType::BALANCED:
            workload_name = "BALANCED";
            break;
        case WorkloadType::SCAN_HEAVY:
            workload_name = "SCAN_HEAVY";
            break;
        }

        std::cout << std::setw(15) << workload_name
                  << std::setw(10) << result.operation_count
                  << std::setw(10) << result.total_time_ms
                  << std::setw(10) << result.avg_read_time_ms
                  << std::setw(10) << result.avg_write_time_ms
                  << std::setw(10) << result.avg_scan_time_ms
                  << std::setw(10) << result.compaction_count
                  << std::setw(10) << result.total_io_reads
                  << std::setw(10) << result.total_io_writes
                  << std::setw(10) << result.final_sstable_count
                  << std::endl;
    }

    std::cout << "\nDetailed Results:" << std::endl;

    for (const auto &result : results)
    {
        std::string workload_name;
        switch (result.type)
        {
        case WorkloadType::READ_HEAVY:
            workload_name = "READ_HEAVY";
            break;
        case WorkloadType::WRITE_HEAVY:
            workload_name = "WRITE_HEAVY";
            break;
        case WorkloadType::BALANCED:
            workload_name = "BALANCED";
            break;
        case WorkloadType::SCAN_HEAVY:
            workload_name = "SCAN_HEAVY";
            break;
        }

        std::cout << "\n--- " << workload_name << " Workload ---" << std::endl;
        std::cout << "Total operations: " << result.operation_count << std::endl;
        std::cout << "  - Reads: " << result.read_count << std::endl;
        std::cout << "  - Writes: " << result.write_count << std::endl;
        std::cout << "  - Scans: " << result.scan_count << std::endl;
        std::cout << "Total time: " << result.total_time_ms << " ms" << std::endl;
        std::cout << "Average latencies:" << std::endl;
        std::cout << "  - Read: " << result.avg_read_time_ms << " ms" << std::endl;
        std::cout << "  - Write: " << result.avg_write_time_ms << " ms" << std::endl;
        std::cout << "  - Scan: " << result.avg_scan_time_ms << " ms" << std::endl;
        std::cout << "Compactions performed: " << result.compaction_count << std::endl;
        std::cout << "I/O operations:" << std::endl;
        std::cout << "  - Reads: " << result.total_io_reads << std::endl;
        std::cout << "  - Writes: " << result.total_io_writes << std::endl;
        std::cout << "Final SSTable count: " << result.final_sstable_count << std::endl;

        // Calculate throughput
        double throughput = result.operation_count * 1000.0 / result.total_time_ms;
        std::cout << "Throughput: " << throughput << " ops/sec" << std::endl;
    }
}

int main()
{
    std::cout << "Running LSM-Tree Workload Experiments..." << std::endl;

    // Number of operations per workload
    const size_t OPERATION_COUNT = 50000;

    // Run experiments for each workload type
    std::vector<ExperimentResult> results;

    results.push_back(runWorkloadExperiment(WorkloadType::READ_HEAVY, OPERATION_COUNT));
    results.push_back(runWorkloadExperiment(WorkloadType::WRITE_HEAVY, OPERATION_COUNT));
    results.push_back(runWorkloadExperiment(WorkloadType::BALANCED, OPERATION_COUNT));
    results.push_back(runWorkloadExperiment(WorkloadType::SCAN_HEAVY, OPERATION_COUNT));

    // Print results
    printResults(results);

    std::cout << "\nAll workload experiments completed!" << std::endl;
    return 0;
}