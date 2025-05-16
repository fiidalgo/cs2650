#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>

#include "thread_pool.h"
#include "constants.h"

namespace lsm
{

    class Server
    {
    public:
        Server(int port = constants::DEFAULT_PORT);
        ~Server();

        // Deleted copy/move constructors and assignment operators
        Server(const Server &) = delete;
        Server &operator=(const Server &) = delete;
        Server(Server &&) = delete;
        Server &operator=(Server &&) = delete;

        // Start the server
        void start();

        // Stop the server
        void stop();

    private:
        // Handle new client connections
        void handle_connections();

        // Handle client communication
        void handle_client(int client_socket);

        // Process a command from a client
        std::string process_command(const std::string &command);

        int server_socket;
        int port;
        std::atomic<bool> running;

        // Thread pool for handling client requests
        std::unique_ptr<ThreadPool> thread_pool;

        // Thread for accepting new connections
        std::unique_ptr<std::thread> connection_thread;

        // Active client connections
        std::unordered_map<int, std::unique_ptr<std::thread>> clients;
        std::mutex clients_mutex;
    };

} // namespace lsm

#endif // SERVER_H