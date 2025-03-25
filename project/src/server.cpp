#include "common.h"
#include "lsm_tree.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>

using namespace lsm;

// Parse command line arguments
struct ServerConfig
{
    std::string data_dir;
};

ServerConfig parseArgs(int argc, char *argv[])
{
    ServerConfig config;
    config.data_dir = "./data";

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--data-dir" && i + 1 < argc)
        {
            config.data_dir = argv[++i];
        }
    }

    return config;
}

// Split a string by whitespace
std::vector<std::string> splitString(const std::string &str)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (ss >> token)
    {
        tokens.push_back(token);
    }

    return tokens;
}

// Process a command
std::string processCommand(LSMTree &db, const std::string &command)
{
    auto tokens = splitString(command);
    if (tokens.empty())
    {
        return "Error: Empty command";
    }

    std::string cmd = tokens[0];

    if (cmd == "p" || cmd == "put")
    {
        // Put command: p [key] [value]
        if (tokens.size() < 3)
        {
            return "Error: Invalid put command. Usage: p [key] [value]";
        }

        try
        {
            Key key = std::stoll(tokens[1]);
            Value value = std::stoll(tokens[2]);

            Status status = db.put(key, value);
            return statusToString(status);
        }
        catch (const std::exception &e)
        {
            return "Error: " + std::string(e.what());
        }
    }
    else if (cmd == "g" || cmd == "get")
    {
        // Get command: g [key]
        if (tokens.size() < 2)
        {
            return "Error: Invalid get command. Usage: g [key]";
        }

        try
        {
            Key key = std::stoll(tokens[1]);
            Value value;

            Status status = db.get(key, value);
            if (status == Status::OK)
            {
                return std::to_string(value);
            }
            else
            {
                return statusToString(status);
            }
        }
        catch (const std::exception &e)
        {
            return "Error: " + std::string(e.what());
        }
    }
    else if (cmd == "d" || cmd == "delete")
    {
        // Delete command: d [key]
        if (tokens.size() < 2)
        {
            return "Error: Invalid delete command. Usage: d [key]";
        }

        try
        {
            Key key = std::stoll(tokens[1]);

            Status status = db.remove(key);
            return statusToString(status);
        }
        catch (const std::exception &e)
        {
            return "Error: " + std::string(e.what());
        }
    }
    else if (cmd == "r" || cmd == "range")
    {
        // Range command: r [start] [end]
        if (tokens.size() < 3)
        {
            return "Error: Invalid range command. Usage: r [start] [end]";
        }

        try
        {
            Key start_key = std::stoll(tokens[1]);
            Key end_key = std::stoll(tokens[2]);

            std::vector<std::pair<Key, Value>> results;
            Status status = db.range(start_key, end_key, results);

            if (status == Status::OK)
            {
                std::stringstream ss;
                ss << "Found " << results.size() << " results:" << std::endl;

                for (const auto &kv : results)
                {
                    ss << kv.first << ": " << kv.second << std::endl;
                }

                return ss.str();
            }
            else
            {
                return statusToString(status);
            }
        }
        catch (const std::exception &e)
        {
            return "Error: " + std::string(e.what());
        }
    }
    else if (cmd == "l" || cmd == "load")
    {
        // Load command: l [filepath]
        if (tokens.size() < 2)
        {
            return "Error: Invalid load command. Usage: l [filepath]";
        }

        try
        {
            std::string filepath = tokens[1];
            std::ifstream file(filepath);

            if (!file.is_open())
            {
                return "Error: Could not open file " + filepath;
            }

            std::string line;
            size_t line_count = 0;
            size_t success_count = 0;

            while (std::getline(file, line))
            {
                line_count++;
                auto line_tokens = splitString(line);

                if (line_tokens.size() >= 2)
                {
                    try
                    {
                        Key key = std::stoll(line_tokens[0]);
                        Value value = std::stoll(line_tokens[1]);

                        Status status = db.put(key, value);
                        if (status == Status::OK)
                        {
                            success_count++;
                        }
                    }
                    catch (...)
                    {
                        // Skip invalid lines
                    }
                }
            }

            return "Loaded " + std::to_string(success_count) + " of " +
                   std::to_string(line_count) + " entries from " + filepath;
        }
        catch (const std::exception &e)
        {
            return "Error: " + std::string(e.what());
        }
    }
    else if (cmd == "s" || cmd == "stats")
    {
        // Stats command: s
        std::map<std::string, std::string> stats;
        db.getStats(stats);

        std::stringstream ss;
        ss << "LSM-Tree Statistics:" << std::endl;

        for (const auto &stat : stats)
        {
            ss << stat.first << ": " << stat.second << std::endl;
        }

        // Add I/O statistics
        ss << "I/O reads: " << IOTracker::getInstance().getReadCount() << std::endl;
        ss << "I/O writes: " << IOTracker::getInstance().getWriteCount() << std::endl;
        ss << "I/O read bytes: " << IOTracker::getInstance().getReadBytes() << std::endl;
        ss << "I/O write bytes: " << IOTracker::getInstance().getWriteBytes() << std::endl;

        return ss.str();
    }
    else if (cmd == "q" || cmd == "quit" || cmd == "exit")
    {
        // Quit command
        return "QUIT";
    }
    else
    {
        return "Error: Unknown command '" + cmd + "'";
    }
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    ServerConfig config = parseArgs(argc, argv);

    // Ensure data directory exists
    if (!std::filesystem::exists(config.data_dir))
    {
        std::filesystem::create_directories(config.data_dir);
    }

    std::cout << "LSM-Tree Key-Value Store Server" << std::endl;
    std::cout << "===============================" << std::endl;
    std::cout << "Data directory: " << config.data_dir << std::endl;
    std::cout << "Type 'help' for a list of commands, 'quit' to exit" << std::endl;

    // Create LSM-Tree
    LSMTree db(config.data_dir);

    // Command loop
    std::string line;
    while (true)
    {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line == "help")
        {
            std::cout << "Available commands:" << std::endl;
            std::cout << "  p [key] [value]   - Put a key-value pair" << std::endl;
            std::cout << "  g [key]           - Get a value for a key" << std::endl;
            std::cout << "  d [key]           - Delete a key" << std::endl;
            std::cout << "  r [start] [end]   - Range query" << std::endl;
            std::cout << "  l [filepath]      - Load key-value pairs from a file" << std::endl;
            std::cout << "  s                 - Print statistics" << std::endl;
            std::cout << "  q, quit, exit     - Exit the server" << std::endl;
            continue;
        }

        std::string result = processCommand(db, line);

        if (result == "QUIT")
        {
            break;
        }

        std::cout << result << std::endl;
    }

    std::cout << "Server shutting down..." << std::endl;
    return 0;
}