#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <memory>
#include "naive/lsm_tree.h"
#include "server/dsl_parser.h"
#include "naive/memtable.h"

namespace server
{
    /**
     * Server - Handles database operations and client requests
     *
     * The server manages the LSM-Tree and processes commands
     * from the client.
     */
    class Server
    {
    public:
        /**
         * Constructor
         *
         * @param data_dir Directory for database storage
         * @param impl_type Implementation type (naive, compaction, etc.)
         */
        Server(const std::string &data_dir, const std::string &impl_type);

        /**
         * Starts the server
         */
        void start();

        /**
         * Checks if the server is running
         */
        bool is_running() const;

        /**
         * Stops the server
         */
        void stop();

        // Main server loop (console mode)
        void run();

        // Socket server mode (listens for network connections)
        void run_socket_server(int port = 9090);

        // Process a command and return the result
        std::string execute_command(const std::string &command_str);

    private:
        // Get database statistics
        std::string get_stats();

        // The LSM-Tree instance
        std::unique_ptr<naive::LSMTree> lsm_tree_;

        // Server state
        bool running_;

        // Data directory
        std::string data_dir_;

        // Implementation type
        std::string impl_type_;

        std::unique_ptr<DSLParser> parser_;
        naive::MemTable db_; // Simple in-memory database for testing
    };

} // namespace server

#endif // SERVER_H