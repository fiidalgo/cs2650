#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>
#include <thread>

namespace lsm
{
    namespace constants
    {

        // Network settings
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

        // LSM-Tree command settings
        constexpr char CMD_PUT = 'p';
        constexpr char CMD_GET = 'g';
        constexpr char CMD_RANGE = 'r';
        constexpr char CMD_DELETE = 'd';
        constexpr char CMD_LOAD = 'l';
        constexpr char CMD_STATS = 's';
        constexpr char CMD_HELP = 'h';
        constexpr const char *CMD_EXIT = "q";
        constexpr const char *CMD_DELIMITER = "\r\n";

        // Help text for client
        constexpr const char *HELP_TEXT = R"(
LSM-Tree
========
Available commands:

p [key] [value]     - Put a key-value pair into the tree
g [key]             - Get the value associated with a key
r [start] [end]     - Range query for keys from start (inclusive) to end (exclusive)
d [key]             - Delete a key-value pair
l [filepath]        - Load key-value pairs from a binary file
s                   - Print statistics about the tree
h                   - Show this help message
q                   - Disconnect from the server
)";

    }
}

#endif