#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>
#include "compaction/lsm_tree.h"

using namespace lsm_tree;
using json = nlohmann::json;

class CompactionTuningTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use the specified data directory
        test_dir_ = "../data/compaction/tuning_" + std::to_string(std::time(nullptr));
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    // Helper to generate test data
    void generateTestData(CompactionLSMTree& tree, size_t num_keys, bool flush_frequently = false) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 1000000);

        size_t flush_interval = flush_frequently ? 10000 : num_keys;
        
        for (size_t i = 0; i < num_keys; ++i) {
            std::string key = "key_" + std::to_string(dis(gen));
            std::string value = "value_" + std::to_string(dis(gen));
            tree.put(key, value);
            
            // Flush more frequently to trigger more compactions
            if (flush_frequently && i % flush_interval == 0) {
                tree.flush();
            }
        }
        tree.flush();
    }

    // Helper to force the creation of multiple levels
    void forceMultipleLevels(CompactionLSMTree& tree, size_t num_operations) {
        // Generate data and flush frequently to trigger compactions
        for (size_t i = 0; i < 5; ++i) {  // 5 waves of data
            generateTestData(tree, num_operations / 5, true);
            tree.compact();  // Explicitly trigger compaction
        }
    }

    // Helper to measure operation latency
    template<typename Func>
    double measureLatency(Func operation) {
        auto start = std::chrono::high_resolution_clock::now();
        operation();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    std::string test_dir_;
};

// Test different L0 thresholds
TEST_F(CompactionTuningTest, L0ThresholdTuning) {
    std::vector<size_t> thresholds = {2, 4, 6, 8};
    std::vector<double> get_latencies;
    std::vector<double> write_throughputs;
    std::vector<double> compaction_frequencies;
    std::vector<size_t> total_bytes_written;

    for (auto threshold : thresholds) {
        CompactionLSMTree tree(test_dir_, threshold);
        
        // Generate initial data
        generateTestData(tree, 500000, true);
        
        // Measure write throughput
        auto write_start = std::chrono::high_resolution_clock::now();
        generateTestData(tree, 500000, true);
        auto write_end = std::chrono::high_resolution_clock::now();
        double write_time = std::chrono::duration<double>(write_end - write_start).count();
        write_throughputs.push_back(500000 / write_time);
        
        // Measure read latency
        double total_latency = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);
        
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(dis(gen));
            total_latency += measureLatency([&]() { tree.get(key); });
        }
        get_latencies.push_back(total_latency / 1000);
        
        // Record metrics
        compaction_frequencies.push_back(tree.getCompactionFrequency());
        total_bytes_written.push_back(tree.getTotalBytesWritten());
        
        // Log detailed statistics for analysis
        std::cout << "L0 Threshold: " << threshold << std::endl;
        std::cout << tree.getStats() << std::endl;
    }

    // Save results to JSON
    json results;
    results["thresholds"] = thresholds;
    results["get_latencies"] = get_latencies;
    results["write_throughputs"] = write_throughputs;
    results["compaction_frequencies"] = compaction_frequencies;
    results["total_bytes_written"] = total_bytes_written;

    std::ofstream out(test_dir_ + "/l0_threshold_tuning.json");
    out << results.dump(2);
    
    std::cout << "L0 threshold tuning results saved to " << test_dir_ << "/l0_threshold_tuning.json" << std::endl;
}

// Test different size ratios
TEST_F(CompactionTuningTest, SizeRatioTuning) {
    std::vector<size_t> ratios = {5, 10, 20};
    std::vector<double> get_latencies;
    std::vector<double> write_throughputs;
    std::vector<size_t> total_bytes_written;
    std::vector<size_t> level_counts;
    
    for (auto ratio : ratios) {
        CompactionLSMTree tree(test_dir_, 4, ratio);
        
        // Force the creation of multiple levels to truly test the size ratio
        forceMultipleLevels(tree, 1000000);
        
        // Measure write throughput
        auto write_start = std::chrono::high_resolution_clock::now();
        generateTestData(tree, 500000, true);
        auto write_end = std::chrono::high_resolution_clock::now();
        double write_time = std::chrono::duration<double>(write_end - write_start).count();
        write_throughputs.push_back(500000 / write_time);
        
        // Measure read latency
        double total_latency = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);
        
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(dis(gen));
            total_latency += measureLatency([&]() { tree.get(key); });
        }
        get_latencies.push_back(total_latency / 1000);
        
        // Record metrics
        total_bytes_written.push_back(tree.getTotalBytesWritten());
        
        // Count how many levels were created
        size_t level_count = 0;
        while (tree.getSSTableCount(level_count) > 0) {
            level_count++;
        }
        level_counts.push_back(level_count);
        
        // Log detailed statistics for analysis
        std::cout << "Size Ratio: " << ratio << std::endl;
        std::cout << tree.getStats() << std::endl;
    }

    // Save results to JSON
    json results;
    results["ratios"] = ratios;
    results["get_latencies"] = get_latencies;
    results["write_throughputs"] = write_throughputs;
    results["total_bytes_written"] = total_bytes_written;
    results["level_counts"] = level_counts;

    std::ofstream out(test_dir_ + "/size_ratio_tuning.json");
    out << results.dump(2);
    
    std::cout << "Size ratio tuning results saved to " << test_dir_ << "/size_ratio_tuning.json" << std::endl;
}

// Test different compaction policies
TEST_F(CompactionTuningTest, PolicyTuning) {
    std::vector<std::string> policies = {"leveling", "tiered"};
    std::vector<double> get_latencies;
    std::vector<double> write_throughputs;
    std::vector<double> compaction_frequencies;
    std::vector<size_t> total_bytes_written;

    for (const auto& policy : policies) {
        CompactionLSMTree tree(test_dir_, 4, 10, policy);
        
        // Generate data with multiple compactions
        forceMultipleLevels(tree, 1000000);
        
        // Measure write throughput
        auto write_start = std::chrono::high_resolution_clock::now();
        generateTestData(tree, 500000, true);
        auto write_end = std::chrono::high_resolution_clock::now();
        double write_time = std::chrono::duration<double>(write_end - write_start).count();
        write_throughputs.push_back(500000 / write_time);
        
        // Measure read latency
        double total_latency = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);
        
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(dis(gen));
            total_latency += measureLatency([&]() { tree.get(key); });
        }
        get_latencies.push_back(total_latency / 1000);
        
        // Record metrics
        compaction_frequencies.push_back(tree.getCompactionFrequency());
        total_bytes_written.push_back(tree.getTotalBytesWritten());
        
        // Log detailed statistics for analysis
        std::cout << "Compaction Policy: " << policy << std::endl;
        std::cout << tree.getStats() << std::endl;
    }

    // Save results to JSON
    json results;
    results["policies"] = policies;
    results["get_latencies"] = get_latencies;
    results["write_throughputs"] = write_throughputs;
    results["compaction_frequencies"] = compaction_frequencies;
    results["total_bytes_written"] = total_bytes_written;

    std::ofstream out(test_dir_ + "/policy_tuning.json");
    out << results.dump(2);
    
    std::cout << "Policy tuning results saved to " << test_dir_ << "/policy_tuning.json" << std::endl;
}

// Test range query performance
TEST_F(CompactionTuningTest, RangeQueryTuning) {
    std::vector<size_t> range_sizes = {100, 500, 1000};
    std::vector<double> range_latencies;
    std::vector<size_t> bytes_read;

    CompactionLSMTree tree(test_dir_, 4, 10);
    forceMultipleLevels(tree, 1000000);

    for (auto size : range_sizes) {
        double total_latency = 0;
        size_t bytes_before = tree.getTotalBytesRead();
        
        for (int i = 0; i < 100; ++i) {
            std::string start_key = "key_" + std::to_string(i * 1000);
            std::string end_key = "key_" + std::to_string(i * 1000 + size);
            
            total_latency += measureLatency([&]() {
                tree.range(start_key, end_key, [](const std::string&, const std::string&) {});
            });
        }
        
        size_t bytes_after = tree.getTotalBytesRead();
        
        range_latencies.push_back(total_latency / 100);
        bytes_read.push_back((bytes_after - bytes_before) / 100);
    }

    // Save results to JSON
    json results;
    results["range_sizes"] = range_sizes;
    results["range_latencies"] = range_latencies;
    results["bytes_read"] = bytes_read;

    std::ofstream out(test_dir_ + "/range_query_tuning.json");
    out << results.dump(2);
    
    std::cout << "Range query tuning results saved to " << test_dir_ << "/range_query_tuning.json" << std::endl;
} 