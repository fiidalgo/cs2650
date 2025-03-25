#include "common.h"
#include "lsm_tree.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sys/resource.h>

using namespace lsm;

// Experiment parameters
struct ExperimentConfig
{
    size_t num_keys;
    size_t key_range;
    size_t num_queries;
    std::string data_dir;
    bool run_put_experiment;
    bool run_get_experiment;
    bool clear_data_dir;
    size_t value_size;
};

// Experiment results
struct ExperimentResults
{
    // Put results
    double put_time_ms;
    size_t put_io_reads;
    size_t put_io_writes;
    size_t put_io_read_bytes;
    size_t put_io_write_bytes;

    // Get results
    double get_time_ms;
    size_t get_io_reads;
    size_t get_io_writes;
    size_t get_io_read_bytes;
    size_t get_io_write_bytes;
    size_t get_hits;
    size_t get_misses;

    // Range query results
    double range_time_ms;
    size_t range_io_reads;
    size_t range_io_writes;
    size_t range_io_read_bytes;
    size_t range_io_write_bytes;
    size_t range_results_count;

    ExperimentConfig config;
};

// Run put experiment
void runPutExperiment(LSMTree &db, const ExperimentConfig &config, ExperimentResults &results)
{
    std::cout << "Running PUT experiment with " << config.num_keys << " keys..." << std::endl;

    // Generate random keys
    std::vector<Key> keys(config.num_keys);
    std::mt19937_64 gen(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<Key> key_dist(1, config.key_range);

    for (size_t i = 0; i < config.num_keys; i++)
    {
        keys[i] = key_dist(gen);
    }

    // Reset IO tracker
    IOTracker::getInstance().reset();

    // Measure put operations
    auto start_time = std::chrono::high_resolution_clock::now();

    // Insert keys
    for (size_t i = 0; i < config.num_keys; i++)
    {
        Key key = keys[i];
        Value value = static_cast<Value>(i);
        Status status = db.put(key, value);
        if (status != Status::OK)
        {
            std::cerr << "Error putting key " << key << ": " << statusToString(status) << std::endl;
        }
    }

    // Explicitly flush all memtables to disk
    std::cout << "  Flushing memtables to disk..." << std::endl;
    db.flushAllMemTables();

    // End timing
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Record results
    results.put_time_ms = duration.count();
    results.put_io_reads = IOTracker::getInstance().getReadCount();
    results.put_io_writes = IOTracker::getInstance().getWriteCount();
    results.put_io_read_bytes = IOTracker::getInstance().getReadBytes();
    results.put_io_write_bytes = IOTracker::getInstance().getWriteBytes();

    // Print results
    std::cout << "PUT experiment completed in " << results.put_time_ms << " ms" << std::endl;
    std::cout << "I/O operations: " << results.put_io_reads << " reads, "
              << results.put_io_writes << " writes" << std::endl;
    std::cout << "I/O bytes: " << results.put_io_read_bytes << " read, "
              << results.put_io_write_bytes << " written" << std::endl;
    std::cout << "Throughput: " << (config.num_keys * 1000.0 / results.put_time_ms)
              << " ops/sec" << std::endl;
    std::cout << "Write amplification: "
              << (double)results.put_io_write_bytes / (config.num_keys * (sizeof(Key) + sizeof(Value)))
              << std::endl;
    std::cout << std::endl;
}

// Run get experiment
void runGetExperiment(LSMTree &db, const ExperimentConfig &config, ExperimentResults &results)
{
    std::cout << "Running GET experiment with " << config.num_queries << " queries..." << std::endl;

    // Generate random keys for queries
    std::vector<Key> query_keys(config.num_queries);
    std::mt19937_64 gen(43); // Different seed from put experiment
    std::uniform_int_distribution<Key> key_dist(1, config.key_range);

    for (size_t i = 0; i < config.num_queries; i++)
    {
        query_keys[i] = key_dist(gen);
    }

    // Reset IO tracker
    IOTracker::getInstance().reset();

    // Measure get operations
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t hits = 0;
    size_t misses = 0;

    for (size_t i = 0; i < config.num_queries; i++)
    {
        Key key = query_keys[i];
        Value value;
        Status status = db.get(key, value);

        if (status == Status::OK)
        {
            hits++;
        }
        else if (status == Status::NOT_FOUND)
        {
            misses++;
        }
        else
        {
            std::cerr << "Error getting key " << key << ": " << statusToString(status) << std::endl;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Record results
    results.get_time_ms = duration.count();
    results.get_io_reads = IOTracker::getInstance().getReadCount();
    results.get_io_writes = IOTracker::getInstance().getWriteCount();
    results.get_io_read_bytes = IOTracker::getInstance().getReadBytes();
    results.get_io_write_bytes = IOTracker::getInstance().getWriteBytes();
    results.get_hits = hits;
    results.get_misses = misses;

    // Print results
    std::cout << "GET experiment completed in " << results.get_time_ms << " ms" << std::endl;
    std::cout << "I/O operations: " << results.get_io_reads << " reads, "
              << results.get_io_writes << " writes" << std::endl;
    std::cout << "I/O bytes: " << results.get_io_read_bytes << " read, "
              << results.get_io_write_bytes << " written" << std::endl;
    std::cout << "Throughput: " << (config.num_queries * 1000.0 / results.get_time_ms)
              << " ops/sec" << std::endl;
    std::cout << "Hit rate: " << (hits * 100.0 / config.num_queries) << "%" << std::endl;
    std::cout << "Average I/O per query: "
              << (double)results.get_io_reads / config.num_queries << " reads, "
              << (double)results.get_io_read_bytes / config.num_queries << " bytes" << std::endl;
    std::cout << std::endl;
}

// Add a range query experiment function
void runRangeExperiment(LSMTree &db, const ExperimentConfig &config, ExperimentResults &results)
{
    std::cout << "Running RANGE experiment with " << config.num_queries << " queries..." << std::endl;

    // Generate random range queries
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Key> key_dist(0, config.key_range - 1);

    // Range size distribution (small, medium, large ranges)
    std::vector<size_t> range_sizes = {10, 100, 1000};
    std::uniform_int_distribution<size_t> range_size_dist(0, range_sizes.size() - 1);

    // Reset IO tracking
    IOTracker::getInstance().reset();

    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t total_results = 0;

    // Run range queries
    for (size_t i = 0; i < config.num_queries; i++)
    {
        Key start_key = key_dist(gen);
        size_t range_size = range_sizes[range_size_dist(gen)];
        Key end_key = start_key + range_size;

        std::vector<std::pair<Key, Value>> results;
        db.range(start_key, end_key, results);

        total_results += results.size();
    }

    // End timing
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Record results
    results.range_time_ms = duration.count();
    results.range_io_reads = IOTracker::getInstance().getReadCount();
    results.range_io_writes = IOTracker::getInstance().getWriteCount();
    results.range_io_read_bytes = IOTracker::getInstance().getReadBytes();
    results.range_io_write_bytes = IOTracker::getInstance().getWriteBytes();
    results.range_results_count = total_results;

    // Print results
    std::cout << "RANGE experiment completed in " << results.range_time_ms << " ms" << std::endl;
    std::cout << "I/O operations: " << results.range_io_reads << " reads, "
              << results.range_io_writes << " writes" << std::endl;
    std::cout << "I/O bytes: " << results.range_io_read_bytes << " read, "
              << results.range_io_write_bytes << " written" << std::endl;

    double throughput = (results.range_time_ms > 0) ? (config.num_queries * 1000.0 / results.range_time_ms) : std::numeric_limits<double>::infinity();

    std::cout << "Throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "Average results per query: " << (double)total_results / config.num_queries << std::endl;
    std::cout << "Average I/O per query: "
              << (double)results.range_io_reads / config.num_queries << " reads, "
              << (double)results.range_io_read_bytes / config.num_queries << " bytes" << std::endl;
    std::cout << std::endl;
}

// Clear data directory
void clearDataDir(const std::string &data_dir)
{
    std::cout << "Clearing data directory: " << data_dir << std::endl;

    try
    {
        if (std::filesystem::exists(data_dir))
        {
            for (const auto &entry : std::filesystem::directory_iterator(data_dir))
            {
                std::filesystem::remove_all(entry.path());
            }
        }
        else
        {
            std::filesystem::create_directories(data_dir);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error clearing data directory: " << e.what() << std::endl;
    }
}

// Save results to CSV
void saveResultsToCsv(const ExperimentResults &results, const ExperimentConfig &config, const std::string &filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
        return;
    }

    // Write header
    file << "operation,num_keys,key_range,num_queries,time_ms,throughput,io_reads,io_writes,io_read_bytes,io_write_bytes,results_count" << std::endl;

    // Write PUT results
    double put_throughput = (results.put_time_ms > 0) ? (config.num_keys * 1000.0 / results.put_time_ms) : std::numeric_limits<double>::infinity();
    file << "PUT," << config.num_keys << "," << config.key_range << "," << config.num_keys << ","
         << results.put_time_ms << "," << put_throughput << ","
         << results.put_io_reads << "," << results.put_io_writes << ","
         << results.put_io_read_bytes << "," << results.put_io_write_bytes << ",0" << std::endl;

    // Write GET results
    double get_throughput = (results.get_time_ms > 0) ? (config.num_queries * 1000.0 / results.get_time_ms) : std::numeric_limits<double>::infinity();
    file << "GET," << config.num_keys << "," << config.key_range << "," << config.num_queries << ","
         << results.get_time_ms << "," << get_throughput << ","
         << results.get_io_reads << "," << results.get_io_writes << ","
         << results.get_io_read_bytes << "," << results.get_io_write_bytes << ",0" << std::endl;

    // Write RANGE results
    double range_throughput = (results.range_time_ms > 0) ? (config.num_queries * 1000.0 / results.range_time_ms) : std::numeric_limits<double>::infinity();
    file << "RANGE," << config.num_keys << "," << config.key_range << "," << config.num_queries << ","
         << results.range_time_ms << "," << range_throughput << ","
         << results.range_io_reads << "," << results.range_io_writes << ","
         << results.range_io_read_bytes << "," << results.range_io_write_bytes << ","
         << results.range_results_count << std::endl;

    file.close();
    std::cout << "Results saved to " << filename << std::endl;
}

// Run experiments with different data sizes
void runDataSizeExperiments()
{
    std::vector<size_t> data_sizes = {1000};
    std::string base_dir = "../data/experiment_";

    for (size_t data_size : data_sizes)
    {
        std::string dir = base_dir + std::to_string(data_size);
        clearDataDir(dir);

        // Create experiment configuration
        ExperimentConfig config;
        config.num_keys = data_size;
        config.key_range = data_size * 10; // Keys are in range [0, 10*data_size)
        config.num_queries = data_size;    // Number of queries equals number of keys
        config.value_size = 8;             // 8-byte values
        config.data_dir = dir;

        // Create LSM-Tree instance
        LSMTree db(config.data_dir);

        // Run experiments
        ExperimentResults results;
        results.config = config;

        // Run PUT experiment
        runPutExperiment(db, config, results);

        // Run GET experiment
        runGetExperiment(db, config, results);

        // Run RANGE experiment
        runRangeExperiment(db, config, results);

        // Save results to CSV
        std::string csv_filename = "../data/results_" + std::to_string(data_size) + ".csv";
        saveResultsToCsv(results, config, csv_filename);

        std::cout << "Experiments completed with " << data_size << " keys." << std::endl;
        std::cout << "=======================================================" << std::endl;
    }
}

// Get current memory usage in KB
size_t getCurrentMemoryUsage()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    // Return resident set size in KB
    return usage.ru_maxrss;
}

// Test the impact of compaction on performance
void runCompactionExperiment(const std::string &data_dir)
{
    std::cout << "Running COMPACTION experiment..." << std::endl;

    // Create a new data directory for this experiment
    std::string compaction_dir = data_dir + "_compaction";
    clearDataDir(compaction_dir);

    // Create LSM-Tree instance
    LSMTree db(compaction_dir);

    // Configuration - increased batch size and number of batches
    const size_t num_keys = 100000;  // Increased from 10000
    const size_t batch_size = 10000; // Increased from 1000
    const size_t num_batches = 10;

    // Results storage
    std::vector<double> batch_times;
    std::vector<size_t> io_reads;
    std::vector<size_t> io_writes;
    std::vector<size_t> io_read_bytes;
    std::vector<size_t> io_write_bytes;
    std::vector<size_t> memory_usage_kb;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Key> key_dist(0, num_keys * 10);

    // Insert data in batches and measure performance
    for (size_t batch = 0; batch < num_batches; batch++)
    {
        std::cout << "Batch " << batch + 1 << "/" << num_batches << std::endl;

        // Reset IO tracking
        IOTracker::getInstance().reset();

        // Start timing
        auto start_time = std::chrono::high_resolution_clock::now();

        // Insert batch of data
        for (size_t i = 0; i < batch_size; i++)
        {
            Key key = key_dist(gen);
            Value value = static_cast<Value>(i);
            db.put(key, value);

            // Force a flush every 1000 operations to ensure disk activity
            if (i > 0 && i % 1000 == 0)
            {
                // This is a hack to try to force a flush - we're calling get()
                // which might trigger internal operations in the LSM-Tree
                Value result;
                db.get(key, result);
            }
        }

        // Explicitly flush all memtables to disk
        std::cout << "  Flushing memtables to disk..." << std::endl;
        db.flushAllMemTables();

        // End timing
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Get memory usage
        size_t memory_kb = getCurrentMemoryUsage();
        memory_usage_kb.push_back(memory_kb);

        // Record results
        batch_times.push_back(duration.count());
        io_reads.push_back(IOTracker::getInstance().getReadCount());
        io_writes.push_back(IOTracker::getInstance().getWriteCount());
        io_read_bytes.push_back(IOTracker::getInstance().getReadBytes());
        io_write_bytes.push_back(IOTracker::getInstance().getWriteBytes());

        // Print batch results
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  I/O: " << IOTracker::getInstance().getReadCount() << " reads, "
                  << IOTracker::getInstance().getWriteCount() << " writes" << std::endl;
        std::cout << "  I/O bytes: " << IOTracker::getInstance().getReadBytes() << " read, "
                  << IOTracker::getInstance().getWriteBytes() << " written" << std::endl;
        std::cout << "  Memory usage: " << memory_kb << " KB" << std::endl;
    }

    // Save compaction results to CSV
    std::string csv_filename = "../data/compaction_results.csv";
    std::ofstream file(csv_filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file " << csv_filename << " for writing." << std::endl;
        return;
    }

    // Write header
    file << "batch,time_ms,io_reads,io_writes,io_read_bytes,io_write_bytes,memory_kb" << std::endl;

    // Write batch results
    for (size_t i = 0; i < num_batches; i++)
    {
        file << i + 1 << "," << batch_times[i] << ","
             << io_reads[i] << "," << io_writes[i] << ","
             << io_read_bytes[i] << "," << io_write_bytes[i] << ","
             << memory_usage_kb[i] << std::endl;
    }

    file.close();
    std::cout << "Compaction results saved to " << csv_filename << std::endl;
    std::cout << "=======================================================" << std::endl;
}

// Test the impact of different value sizes on performance
void runValueSizeExperiment()
{
    std::cout << "Running VALUE SIZE experiment..." << std::endl;

    // Different value sizes to test (in bytes)
    std::vector<size_t> value_sizes = {8, 64, 256, 1024, 4096};

    // Configuration
    const size_t num_keys = 10000;
    const size_t key_range = num_keys * 10;

    // Results storage
    std::vector<double> put_times;
    std::vector<double> get_times;
    std::vector<size_t> put_io_reads;
    std::vector<size_t> put_io_writes;
    std::vector<size_t> get_io_reads;
    std::vector<size_t> get_io_writes;
    std::vector<size_t> memory_usages;

    for (size_t value_size : value_sizes)
    {
        std::cout << "Testing with value size: " << value_size << " bytes" << std::endl;

        // Create a new data directory for this experiment
        std::string data_dir = "../data/experiment_valuesize_" + std::to_string(value_size);
        clearDataDir(data_dir);

        // Create LSM-Tree instance
        LSMTree db(data_dir);

        // Generate random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<Key> key_dist(0, key_range - 1);

        // Create a value with the specified size
        std::vector<uint8_t> value_data(value_size, 0);
        for (size_t i = 0; i < value_size; i++)
        {
            value_data[i] = static_cast<uint8_t>(i % 256);
        }
        Value value = 0;
        if (value_size <= sizeof(Value))
        {
            // For small values, just use a number
            value = 42;
        }

        // Reset IO tracking for PUT
        IOTracker::getInstance().reset();

        // Start timing for PUT
        auto start_time = std::chrono::high_resolution_clock::now();

        // Insert data
        for (size_t i = 0; i < num_keys; i++)
        {
            Key key = key_dist(gen);
            db.put(key, value);
        }

        // Explicitly flush all memtables to disk
        std::cout << "  Flushing memtables to disk..." << std::endl;
        db.flushAllMemTables();

        // End timing for PUT
        auto end_time = std::chrono::high_resolution_clock::now();
        auto put_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Record PUT results
        put_times.push_back(put_duration.count());
        put_io_reads.push_back(IOTracker::getInstance().getReadCount());
        put_io_writes.push_back(IOTracker::getInstance().getWriteCount());

        // Get memory usage
        size_t memory_kb = getCurrentMemoryUsage();
        memory_usages.push_back(memory_kb);

        // Print PUT results
        std::cout << "  PUT time: " << put_duration.count() << " ms" << std::endl;
        std::cout << "  PUT I/O: " << IOTracker::getInstance().getReadCount() << " reads, "
                  << IOTracker::getInstance().getWriteCount() << " writes" << std::endl;
        std::cout << "  Memory usage: " << memory_kb << " KB" << std::endl;

        // Reset IO tracking for GET
        IOTracker::getInstance().reset();

        // Start timing for GET
        start_time = std::chrono::high_resolution_clock::now();

        // Perform GET operations
        size_t hits = 0;
        for (size_t i = 0; i < num_keys; i++)
        {
            Key key = key_dist(gen);
            Value result;
            if (db.get(key, result) == Status::OK)
            {
                hits++;
            }
        }

        // End timing for GET
        end_time = std::chrono::high_resolution_clock::now();
        auto get_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Record GET results
        get_times.push_back(get_duration.count());
        get_io_reads.push_back(IOTracker::getInstance().getReadCount());
        get_io_writes.push_back(IOTracker::getInstance().getWriteCount());

        // Print GET results
        std::cout << "  GET time: " << get_duration.count() << " ms" << std::endl;
        std::cout << "  GET I/O: " << IOTracker::getInstance().getReadCount() << " reads, "
                  << IOTracker::getInstance().getWriteCount() << " writes" << std::endl;
        std::cout << "  Hit rate: " << (100.0 * hits / num_keys) << "%" << std::endl;
        std::cout << std::endl;
    }

    // Save value size results to CSV
    std::string csv_filename = "../data/valuesize_results.csv";
    std::ofstream file(csv_filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file " << csv_filename << " for writing." << std::endl;
        return;
    }

    // Write header
    file << "value_size,put_time_ms,get_time_ms,put_io_reads,put_io_writes,get_io_reads,get_io_writes,memory_kb" << std::endl;

    // Write results
    for (size_t i = 0; i < value_sizes.size(); i++)
    {
        file << value_sizes[i] << "," << put_times[i] << "," << get_times[i] << ","
             << put_io_reads[i] << "," << put_io_writes[i] << ","
             << get_io_reads[i] << "," << get_io_writes[i] << ","
             << memory_usages[i] << std::endl;
    }

    file.close();
    std::cout << "Value size results saved to " << csv_filename << std::endl;
    std::cout << "=======================================================" << std::endl;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    std::cout << "LSM-Tree Experiments" << std::endl;
    std::cout << "===================" << std::endl;

    // Run experiments with different data sizes
    runDataSizeExperiments();

    // Run compaction experiment
    runCompactionExperiment("../data/experiment_1000");

    // Run value size experiment
    runValueSizeExperiment();

    return 0;
}