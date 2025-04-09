#ifndef DSL_PARSER_H
#define DSL_PARSER_H

#include <string>
#include <vector>
#include <optional>

namespace server
{
    /**
     * Supported database command types
     */
    enum class CommandType
    {
        PUT,    // Insert or update a key-value pair (p)
        GET,    // Retrieve a value by key (g)
        RANGE,  // Get values in a key range (r)
        DELETE, // Delete a key-value pair (d)
        STATS,  // Print stats about the database (s)
        HELP,   // Show help information (h)
        EXIT,   // Exit the program (e)
        INVALID // Command not recognized
    };

    /**
     * Represents a parsed command from the DSL
     */
    struct Command
    {
        CommandType type;
        std::optional<int> key1;
        std::optional<int> key2;
        std::optional<int> value;
    };

    /**
     * DSL Parser - Parses commands in the database domain-specific language
     */
    class DSLParser
    {
    public:
        DSLParser() = default;

        /**
         * Parses a command string into a structured command
         *
         * @param cmd_str The command string to parse
         * @return The parsed command
         */
        Command parse(const std::string &cmd_str);

        /**
         * Gets help text for the DSL syntax
         *
         * @return String containing help information
         */
        static std::string get_help();

        /**
         * Helper to get string representation of command
         *
         * @param cmd The command to convert to string
         * @return String representation of the command
         */
        static std::string command_to_string(const Command &cmd);
    };

} // namespace server

#endif // DSL_PARSER_H