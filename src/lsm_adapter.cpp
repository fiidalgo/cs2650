#include "../include/lsm_adapter.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

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

        // Get command code (first character)
        char cmd_code = command[0];

        // Process based on command code
        try
        {
            switch (cmd_code)
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
                // Range command
                auto tokens = tokenize(command);
                return handle_range(tokens);
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
        catch (const std::exception &e)
        {
            return std::string("Error: ") + e.what();
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
            tree->load_file(filepath);
            return "File loaded successfully: " + filepath;
        }
        catch (const std::exception &e)
        {
            return std::string("Error loading file: ") + e.what();
        }
    }

    std::string LSMAdapter::handle_stats()
    {
        // Use a limited size stringstream to avoid extremely large responses
        std::stringstream ss;
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