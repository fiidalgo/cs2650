#include "../include/common.h"
#include "../include/lsm_tree.h"
#include <iostream>
#include <chrono>
#include <random>
#include <fstream>
#include <iomanip>
#include <filesystem>

using namespace lsm;

// Enum to distinguish between implementation types
enum class ImplementationType
{
    NAIVE,
    COMPACTION
};

// Structure to store experiment results
struct ExperimentResult
{
    double put_time_ms;
    double seq_get_time_ms;
    double rand_get_time_ms;
    double range_time_ms;
    size_t io_read_count;
    size_t io_write_count;
    size_t sstable_count;
    size_t compaction_count;
};

// Check if directory exists and create it if not
bool ensureDirectoryExists(const std::string &path)
{
    if (!fileExists(path))
    {
        return createDirectory(path);
    }
    return true;
}

// Remove directory if it exists
void removeDirectoryIfExists(const std::string &path)
{
    std::string cmd = "rm -rf " + path;
    system(cmd.c_str());
}

// Factory class to create LSM trees of different types
class LSMTreeFactory
{
public:
    static std::unique_ptr<LSMTree> createTree(ImplementationType type, const std::string &data_dir)
    {
        ensureDirectoryExists(data_dir);

        auto tree = std::make_unique<LSMTree>(data_dir);

        if (type == ImplementationType::NAIVE)
        {
            // Disable compaction for naive implementation
            tree->setCompactionDisabled(true);
        }
        else
        {
            // Enable compaction with default settings
            tree->setCompactionDisabled(false);
            tree->setLevel0Threshold(4);
            tree->setLevelSizeRatio(10);
        }

        return tree;
    }
};

// Run experiments for both implementation types
std::vector<ExperimentResult> runExperiments(
    const std::vector<ImplementationType> &implementations,
    const std::vector<std::string> &data_dirs,
    size_t num_entries,
    size_t num_queries)
{
    std::vector<ExperimentResult> results;

    // Set up random generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<Key> key_dist(1, num_entries);

    for (size_t i = 0; i < implementations.size(); i++)
    {
        auto type = implementations[i];
        auto data_dir = data_dirs[i];

        ExperimentResult result;

        // Create tree
        auto tree = LSMTreeFactory::createTree(type, data_dir);

        // Reset IO tracking
        IOTracker::getInstance().reset();

        // Generate random keys for random access
        std::vector<Key> random_keys;
        for (size_t j = 0; j < num_queries; j++)
        {
            random_keys.push_back(key_dist(gen));
        }

        // 1. Measure PUT performance
        auto put_start = std::chrono::high_resolution_clock::now();

        for (Key k = 1; k <= num_entries; k++)
        {
            tree->put(k, k);
        }

        auto put_end = std::chrono::high_resolution_clock::now();
        result.put_time_ms = std::chrono::duration<double, std::milli>(put_end - put_start).count();

        // Capture IO operations after PUT
        size_t put_read_count = IOTracker::getInstance().getReadCount();
        size_t put_write_count = IOTracker::getInstance().getWriteCount();

        // 2. Measure sequential GET performance
        auto seq_get_start = std::chrono::high_resolution_clock::now();

        for (Key k = 1; k <= num_queries; k++)
        {
            Value value;
            tree->get(k, value);
        }

        auto seq_get_end = std::chrono::high_resolution_clock::now();
        result.seq_get_time_ms = std::chrono::duration<double, std::milli>(seq_get_end - seq_get_start).count();

        // 3. Measure random GET performance
        auto rand_get_start = std::chrono::high_resolution_clock::now();

        for (const auto &key : random_keys)
        {
            Value value;
            tree->get(key, value);
        }

        auto rand_get_end = std::chrono::high_resolution_clock::now();
        result.rand_get_time_ms = std::chrono::duration<double, std::milli>(rand_get_end - rand_get_start).count();

        // 4. Measure RANGE query performance
        auto range_start = std::chrono::high_resolution_clock::now();

        for (size_t j = 0; j < num_queries / 10; j++)
        {
            Key start_key = key_dist(gen);
            Key end_key = start_key + 100;
            std::vector<std::pair<Key, Value>> range_results;
            tree->range(start_key, end_key, range_results);
        }

        auto range_end = std::chrono::high_resolution_clock::now();
        result.range_time_ms = std::chrono::duration<double, std::milli>(range_end - range_start).count();

        // Collect final statistics
        result.io_read_count = IOTracker::getInstance().getReadCount();
        result.io_write_count = IOTracker::getInstance().getWriteCount();
        result.sstable_count = tree->getTotalSSTableCount();
        result.compaction_count = tree->getCompactionCount();

        results.push_back(result);
    }

    return results;
}

// Output results to CSV file
void outputResultsToCSV(
    const std::vector<ImplementationType> &implementations,
    const std::vector<ExperimentResult> &results,
    const std::string &csv_file)
{
    std::ofstream out(csv_file);
    if (!out.is_open())
    {
        std::cerr << "Failed to open output file: " << csv_file << std::endl;
        return;
    }

    // Write header
    out << "implementation,"
        << "put_time_ms,"
        << "seq_get_time_ms,"
        << "rand_get_time_ms,"
        << "range_time_ms,"
        << "io_read_count,"
        << "io_write_count,"
        << "sstable_count,"
        << "compaction_count"
        << std::endl;

    // Write data
    for (size_t i = 0; i < implementations.size(); i++)
    {
        out << (implementations[i] == ImplementationType::NAIVE ? "naive" : "compaction") << ","
            << std::fixed << std::setprecision(2) << results[i].put_time_ms << ","
            << std::fixed << std::setprecision(2) << results[i].seq_get_time_ms << ","
            << std::fixed << std::setprecision(2) << results[i].rand_get_time_ms << ","
            << std::fixed << std::setprecision(2) << results[i].range_time_ms << ","
            << results[i].io_read_count << ","
            << results[i].io_write_count << ","
            << results[i].sstable_count << ","
            << results[i].compaction_count
            << std::endl;
    }

    out.close();
    std::cout << "Results written to " << csv_file << std::endl;
}

int main()
{
    // Configuration
    const size_t NUM_ENTRIES = 1000000; // 1M entries
    const size_t NUM_QUERIES = 50000;   // 50K queries

    // Define experiment directories
    std::vector<std::string> data_dirs = {
        "test_data_naive",
        "test_data_compaction"};

    // Define implementation types
    std::vector<ImplementationType> implementations = {
        ImplementationType::NAIVE,
        ImplementationType::COMPACTION};

    // Remove existing test directories
    for (const auto &dir : data_dirs)
    {
        removeDirectoryIfExists(dir);
    }

    // Run experiments
    std::cout << "Running experiments..." << std::endl;
    auto results = runExperiments(implementations, data_dirs, NUM_ENTRIES, NUM_QUERIES);

    // Output results to CSV
    outputResultsToCSV(implementations, results, "comparison_results.csv");

    return 0;
}