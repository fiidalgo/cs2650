#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>
#include <thread>
#include <cstddef>
#include <cstdint>
#include <atomic>

namespace lsm
{
    namespace constants
    {

        //======================================================================
        // LSM-Tree Configuration
        //======================================================================

        // Size configurations
        constexpr size_t DEFAULT_BUFFER_SIZE_BYTES = 4 * 1024 * 1024; // 4MB
        inline std::atomic<size_t> BUFFER_SIZE_BYTES = DEFAULT_BUFFER_SIZE_BYTES;

        constexpr size_t SIZE_RATIO = 4;
        constexpr int INITIAL_MAX_LEVEL = 6;

        // Compaction parameters
        constexpr int TIERING_THRESHOLD = 4;       // Level 1: Trigger after 4 runs
        constexpr int LAZY_LEVELING_THRESHOLD = 3; // Levels 2-4: Trigger after 3 runs

        // Compaction control flag
        inline std::atomic<bool> COMPACTION_ENABLED = true;

        // File and directory paths
        inline const std::string DATA_DIRECTORY = "data";
        inline const std::string RUN_FILENAME_PREFIX = "run_";

        // Bloom filter and fence pointer settings
        constexpr double TOTAL_FPR = 1.0;  // Expected total false positives
        constexpr size_t PAGE_SIZE = 4096; // 4KB pages for fence pointers

        // Skip list
        constexpr int MAX_SKIP_LIST_HEIGHT = 32;

        //======================================================================
        // Network & Server Settings
        //======================================================================

        constexpr int DEFAULT_PORT = 9090;
        constexpr const char *DEFAULT_HOST = "127.0.0.1";
        constexpr int MAX_CLIENTS = 64;
        constexpr int CONNECTION_QUEUE_SIZE = 10;
        constexpr int BUFFER_SIZE = 4096;

        // Number of threads
        inline int default_thread_count()
        {
            int count = std::thread::hardware_concurrency();
            return count > 0 ? count : 16;
        }

        //======================================================================
        // Command Interface
        //======================================================================

        constexpr char CMD_PUT = 'p';
        constexpr char CMD_GET = 'g';
        constexpr char CMD_RANGE = 'r';
        constexpr char CMD_DELETE = 'd';
        constexpr char CMD_LOAD = 'l';
        constexpr char CMD_STATS = 's';
        constexpr char CMD_HELP = 'h';
        constexpr const char *CMD_EXIT = "q";
        constexpr const char *CMD_DELIMITER = "\r\n";

        //======================================================================
        // Bloom Filter Constants
        //======================================================================

        // Hash function constants
        constexpr uint64_t FNV_PRIME = 1099511628211ULL;
        constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;

        //======================================================================
        // Menu Text
        //======================================================================

        constexpr const char *HELP_TEXT = R"(
LSM-Tree
========
Available commands:

p [key] [value]     - Put a key-value pair into the tree
g [key]             - Get the value associated with a key
r [start] [end]     - Range query for keys from start (inclusive) to end (exclusive)
d [key]             - Delete a key-value pair
l "[filepath]"      - Load key-value pairs from a binary file
s                   - Print statistics about the tree
h                   - Show this help message
q                   - Disconnect from the server
)";
    }
}

#endif