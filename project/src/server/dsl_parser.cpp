#include "server/dsl_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace server
{
    // Helper function to convert a string to lowercase
    std::string to_lower(const std::string &str)
    {
        std::string lower_str = str;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });
        return lower_str;
    }

    // Helper function to split a string by whitespace
    std::vector<std::string> split(const std::string &str)
    {
        std::vector<std::string> tokens;
        std::istringstream iss(str);
        std::string token;

        while (iss >> token)
        {
            tokens.push_back(token);
        }

        return tokens;
    }

    Command DSLParser::parse(const std::string &cmd_str)
    {
        Command cmd;
        cmd.type = CommandType::INVALID;

        if (cmd_str.empty())
        {
            return cmd;
        }

        std::istringstream iss(cmd_str);
        std::string command;
        iss >> command;

        // Convert command to lowercase
        std::string cmd_type = command.substr(0, 1);

        if (cmd_type == "p")
        {
            cmd.type = CommandType::PUT;
            int key, value;
            if (iss >> key >> value)
            {
                cmd.key1 = key;
                cmd.value = value;
            }
            else
            {
                cmd.type = CommandType::INVALID;
            }
        }
        else if (cmd_type == "g")
        {
            cmd.type = CommandType::GET;
            int key;
            if (iss >> key)
            {
                cmd.key1 = key;
            }
            else
            {
                cmd.type = CommandType::INVALID;
            }
        }
        else if (cmd_type == "r")
        {
            cmd.type = CommandType::RANGE;
            int start_key, end_key;
            if (iss >> start_key >> end_key)
            {
                cmd.key1 = start_key;
                cmd.key2 = end_key;
            }
            else
            {
                cmd.type = CommandType::INVALID;
            }
        }
        else if (cmd_type == "d")
        {
            cmd.type = CommandType::DELETE;
            int key;
            if (iss >> key)
            {
                cmd.key1 = key;
            }
            else
            {
                cmd.type = CommandType::INVALID;
            }
        }
        else if (cmd_type == "s")
        {
            cmd.type = CommandType::STATS;
        }
        else if (cmd_type == "h")
        {
            cmd.type = CommandType::HELP;
        }
        else if (cmd_type == "q")
        {
            cmd.type = CommandType::EXIT;
        }

        return cmd;
    }

    std::string DSLParser::command_to_string(const Command &cmd)
    {
        std::stringstream ss;

        switch (cmd.type)
        {
        case CommandType::PUT:
            ss << "PUT key=" << cmd.key1.value() << " value=" << cmd.value.value();
            break;
        case CommandType::GET:
            ss << "GET key=" << cmd.key1.value();
            break;
        case CommandType::RANGE:
            ss << "RANGE start_key=" << cmd.key1.value() << " end_key=" << cmd.key2.value();
            break;
        case CommandType::DELETE:
            ss << "DELETE key=" << cmd.key1.value();
            break;
        case CommandType::STATS:
            ss << "STATS";
            break;
        case CommandType::HELP:
            ss << "HELP";
            break;
        case CommandType::EXIT:
            ss << "EXIT";
            break;
        case CommandType::INVALID:
            ss << "INVALID COMMAND";
            break;
        }

        return ss.str();
    }

    std::string DSLParser::get_help()
    {
        return R"(
LSM-Tree Database Commands:
---------------------------
p <key> <value>     - Insert or update a key-value pair
g <key>             - Retrieve the value for a key
d <key>             - Delete a key-value pair
r <start> <end>     - Get all key-value pairs in range [start, end)
l "/path/to/file"   - Load key-value pairs from a binary file
s                   - Print database statistics
h                   - Show this help information
q                   - Exit the client

Example:
p 1 100            - Store value 100 under key 1
g 1                - Retrieve the value for key 1
r 1 5              - Get all key-value pairs with keys from 1 to 4
)";
    }

} // namespace server