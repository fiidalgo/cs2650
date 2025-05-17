#include "../include/lsm_adapter.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

namespace lsm
{

    LSMAdapter &LSMAdapter::get_instance()
    {
        static LSMAdapter instance;
        std::cout << "LSM Adapter instance accessed" << std::endl;
        return instance;
    }

    LSMAdapter::LSMAdapter()
    {
        // Create the LSM-tree
        tree = std::make_unique<LSMTree>();
        std::cout << "LSM-Tree adapter initialized" << std::endl;
    }

    void LSMAdapter::shutdown()
    {
        if (tree)
        {
            std::cout << "Flushing LSM-Tree and performing any pending compactions..." << std::endl;
            // The tree destructor will flush the buffer, but we can also force compaction
            // to ensure optimal storage before shutdown
            tree->compact();
            std::cout << "LSM-Tree shutdown complete" << std::endl;
        }
        // Tree will be properly destroyed when adapter's destructor runs
    }

    std::string LSMAdapter::process_command(const std::string &command)
    {
        if (command.empty())
        {
            return "Error: Empty command";
        }

        char cmd_type = command[0];

        switch (cmd_type)
        {
        case 'p':
        {
            // Put command
            auto tokens = tokenize(command);
            return handle_put(tokens);
        }

        case 'g':
        {
            // Get command
            auto tokens = tokenize(command);
            return handle_get(tokens);
        }

        case 'r':
        {
            if (command.size() == 1 || command[1] == ' ')
            {
                // Single 'r' is reset stats
                return handle_reset_stats();
            }
            else
            {
                // "range" command would be longer and start with 'r'
                auto tokens = tokenize(command);
                return handle_range(tokens);
            }
        }

        case 'd':
        {
            // Delete command
            auto tokens = tokenize(command);
            return handle_delete(tokens);
        }

        case 'l':
        {
            // Load command
            return handle_load(command);
        }

        case 's':
        {
            // Stats command
            auto tokens = tokenize(command);
            if (tokens.size() > 1)
            {
                return "Error: Stats command takes no arguments";
            }
            return handle_stats();
        }

        default:
            return "Error: Unknown command";
        }
    }

    std::string LSMAdapter::handle_put(const std::vector<std::string> &tokens)
    {
        if (tokens.size() != 3)
        {
            return "Error: Put command requires exactly 2 arguments";
        }

        try
        {
            int64_t key = std::stoll(tokens[1]);
            int64_t value = std::stoll(tokens[2]);

            tree->put(key, value);
            return "Put successful: " + tokens[1] + " -> " + tokens[2];
        }
        catch (const std::exception &e)
        {
            return std::string("Error parsing arguments: ") + e.what();
        }
    }

    std::string LSMAdapter::handle_get(const std::vector<std::string> &tokens)
    {
        if (tokens.size() != 2)
        {
            return "Error: Get command requires exactly 1 argument";
        }

        try
        {
            int64_t key = std::stoll(tokens[1]);

            auto result = tree->get(key);
            if (result)
            {
                return std::to_string(*result);
            }
            else
            {
                return ""; // Empty string for not found
            }
        }
        catch (const std::exception &e)
        {
            return std::string("Error parsing arguments: ") + e.what();
        }
    }

    std::string LSMAdapter::handle_range(const std::vector<std::string> &tokens)
    {
        if (tokens.size() != 3)
        {
            return "Error: Range command requires exactly 2 arguments";
        }

        try
        {
            int64_t start_key = std::stoll(tokens[1]);
            int64_t end_key = std::stoll(tokens[2]);

            if (start_key >= end_key)
            {
                return "Error: Start key must be less than end key";
            }

            auto results = tree->range(start_key, end_key);

            if (results.empty())
            {
                return ""; // Empty string for empty range
            }

            std::stringstream ss;
            for (const auto &pair : results)
            {
                ss << pair.key << ":" << pair.value << " ";
            }

            return ss.str();
        }
        catch (const std::exception &e)
        {
            return std::string("Error parsing arguments: ") + e.what();
        }
    }

    std::string LSMAdapter::handle_delete(const std::vector<std::string> &tokens)
    {
        if (tokens.size() != 2)
        {
            return "Error: Delete command requires exactly 1 argument";
        }

        try
        {
            int64_t key = std::stoll(tokens[1]);

            bool success = tree->remove(key);
            return success ? "Delete successful" : "Delete failed: Key not found";
        }
        catch (const std::exception &e)
        {
            return std::string("Error parsing arguments: ") + e.what();
        }
    }

    std::string LSMAdapter::handle_load(const std::string &command)
    {
        // Extract filepath - need to handle quoted paths
        std::string filepath;
        size_t start_pos = command.find_first_of("\"'");
        if (start_pos == std::string::npos)
        {
            return "Error: Load command requires filepath in quotes";
        }

        size_t end_pos = command.find_first_of("\"'", start_pos + 1);
        if (end_pos == std::string::npos)
        {
            return "Error: Unclosed quote in filepath";
        }

        filepath = command.substr(start_pos + 1, end_pos - start_pos - 1);

        // Check if there's anything after the closing quote (besides whitespace)
        std::string after_path = command.substr(end_pos + 1);
        if (after_path.find_first_not_of(" \t") != std::string::npos)
        {
            return "Error: Unexpected content after filepath";
        }

        // Check if file exists
        if (!fs::exists(filepath))
        {
            return "Error: File not found: " + filepath;
        }

        try
        {
            // Use the optimized bulk loading method instead of the regular load
            tree->bulk_load_file(filepath);
            std::cout << "Bulk load complete, returning success response" << std::endl;
            return "File loaded successfully: " + filepath;
        }
        catch (const std::exception &e)
        {
            return "Error loading file: " + std::string(e.what());
        }
    }

    std::string LSMAdapter::handle_stats()
    {
        // Use a limited size stringstream to avoid extremely large responses
        std::stringstream ss;

        // Add performance statistics section
        ss << "===== Performance Metrics =====" << std::endl;

        // Total operation counts
        ss << "Total Operations:" << std::endl;
        ss << "  Reads: " << get_read_count() << std::endl;
        ss << "  Writes: " << get_write_count() << std::endl;

        // Average operation times
        ss << "Average Operation Time:" << std::endl;
        ss << "  Reads: " << std::fixed << std::setprecision(3) << get_avg_read_time_ms() << " ms/op" << std::endl;
        ss << "  Writes: " << std::fixed << std::setprecision(3) << get_avg_write_time_ms() << " ms/op" << std::endl;

        // Operation throughput (ops/sec)
        double read_throughput = get_avg_read_time_ms() > 0 ? 1000.0 / get_avg_read_time_ms() : 0;
        double write_throughput = get_avg_write_time_ms() > 0 ? 1000.0 / get_avg_write_time_ms() : 0;

        ss << "Operation Throughput:" << std::endl;
        ss << "  Reads: " << std::fixed << std::setprecision(2) << read_throughput << " ops/sec" << std::endl;
        ss << "  Writes: " << std::fixed << std::setprecision(2) << write_throughput << " ops/sec" << std::endl;

        // Add I/O statistics section
        ss << std::endl
           << "===== I/O Statistics =====" << std::endl;
        ss << "Read I/Os: " << get_read_io_count() << std::endl;
        ss << "Write I/Os: " << get_write_io_count() << std::endl;

        // I/O per operation (efficiency metrics)
        double io_per_read = get_read_count() > 0 ? static_cast<double>(get_read_io_count()) / get_read_count() : 0;
        double io_per_write = get_write_count() > 0 ? static_cast<double>(get_write_io_count()) / get_write_count() : 0;

        ss << "I/O Efficiency:" << std::endl;
        ss << "  I/O per read operation: " << std::fixed << std::setprecision(2) << io_per_read << std::endl;
        ss << "  I/O per write operation: " << std::fixed << std::setprecision(2) << io_per_write << std::endl;

        ss << "=========================" << std::endl
           << std::endl;

        // Then add the full tree stats as before
        tree->print_stats(ss);

        // Get the string and check its size
        std::string stats = ss.str();

        // If stats are empty or only contain whitespace, add a message
        if (stats.empty() || stats.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            return "LSM-Tree is empty. No data has been loaded.";
        }

        // If the stats are very large, truncate them and add a warning
        const size_t MAX_STATS_SIZE = 8192; // Limit to 8KB
        if (stats.size() > MAX_STATS_SIZE)
        {
            std::string truncated = stats.substr(0, MAX_STATS_SIZE);
            truncated += "\n\n[WARNING: Stats output was truncated due to size]";
            return truncated;
        }

        return stats;
    }

    std::string LSMAdapter::handle_reset_stats()
    {
        reset_io_stats();
        reset_timing_stats();
        return "Statistics reset successfully";
    }

    std::vector<std::string> LSMAdapter::tokenize(const std::string &command) const
    {
        std::vector<std::string> tokens;
        std::istringstream iss(command);
        std::string token;

        while (iss >> token)
        {
            tokens.push_back(token);
        }

        return tokens;
    }

} // namespace lsm