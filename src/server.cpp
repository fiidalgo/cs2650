#include "../include/server.h"

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <vector>

namespace lsm
{

    Server::Server(int port)
        : server_socket(-1),
          port(port),
          running(false)
    {
        thread_pool = std::make_unique<ThreadPool>(constants::default_thread_count());
    }

    Server::~Server()
    {
        stop();
    }

    void Server::start()
    {
        // Already running
        if (running.load())
        {
            return;
        }

        // Create socket
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1)
        {
            throw std::runtime_error("Failed to create socket");
        }

        // Set socket options to reuse address
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            close(server_socket);
            throw std::runtime_error("Failed to set socket options");
        }

        // Set up server address
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        // Bind the socket
        if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            close(server_socket);
            throw std::runtime_error("Failed to bind socket to port " + std::to_string(port));
        }

        // Listen for connections
        if (listen(server_socket, constants::CONNECTION_QUEUE_SIZE) < 0)
        {
            close(server_socket);
            throw std::runtime_error("Failed to listen on socket");
        }

        running.store(true);
        std::cout << "Server started on port " << port << std::endl;

        // Start connection handling thread
        connection_thread = std::make_unique<std::thread>(&Server::handle_connections, this);
    }

    void Server::stop()
    {
        if (!running.load())
        {
            return;
        }

        running.store(false);

        // Close the server socket to unblock the accept() call
        if (server_socket != -1)
        {
            close(server_socket);
            server_socket = -1;
        }

        // Wait for the connection handling thread to finish
        if (connection_thread && connection_thread->joinable())
        {
            connection_thread->join();
        }

        // Close all client connections
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto &client : clients)
            {
                close(client.first);
                if (client.second && client.second->joinable())
                {
                    client.second->join();
                }
            }
            clients.clear();
        }

        std::cout << "Server stopped" << std::endl;
    }

    void Server::handle_connections()
    {
        while (running.load())
        {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);

            // Accept a new connection
            int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket < 0)
            {
                if (running.load())
                {
                    std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
                }
                continue;
            }

            // Check if we've reached the maximum number of clients
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                if (clients.size() >= constants::MAX_CLIENTS)
                {
                    std::string error_msg = "Server is full, try again later\r\n";
                    send(client_socket, error_msg.c_str(), error_msg.length(), 0);
                    close(client_socket);
                    continue;
                }
            }

            // Log new connection
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "New connection from: " << client_ip << ":" << ntohs(client_addr.sin_port)
                      << " (socket: " << client_socket << ")" << std::endl;

            // Create a thread to handle this client
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients[client_socket] = std::make_unique<std::thread>(&Server::handle_client, this, client_socket);
            }
        }
    }

    void Server::handle_client(int client_socket)
    {
        char buffer[constants::BUFFER_SIZE];
        std::string command_buffer;

        while (running.load())
        {
            std::memset(buffer, 0, constants::BUFFER_SIZE);
            ssize_t bytes_read = recv(client_socket, buffer, constants::BUFFER_SIZE - 1, 0);

            if (bytes_read <= 0)
            {
                // Connection closed or error
                break;
            }

            // Add received data to the command buffer
            command_buffer.append(buffer, bytes_read);

            // Process complete commands (delimited by \r\n)
            size_t delimiter_pos;
            while ((delimiter_pos = command_buffer.find(constants::CMD_DELIMITER)) != std::string::npos)
            {
                // Extract the command
                std::string command = command_buffer.substr(0, delimiter_pos);
                command_buffer.erase(0, delimiter_pos + strlen(constants::CMD_DELIMITER));

                // Check for exit command
                if (command == constants::CMD_EXIT)
                {
                    std::cout << "Client requested disconnect (socket: " << client_socket << ")" << std::endl;
                    goto cleanup; // Break out of both loops
                }

                // Submit command to thread pool
                auto future = thread_pool->enqueue([this, command, client_socket]()
                                                   {
                std::string response = this->process_command(command);
                response += constants::CMD_DELIMITER;  // Add delimiter to response
                send(client_socket, response.c_str(), response.length(), 0);
                return true; });
            }
        }

    cleanup:
        // Close socket and remove from clients map
        close(client_socket);

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = clients.find(client_socket);
            if (it != clients.end())
            {
                std::thread *client_thread = it->second.get();
                // Detach the thread since we're inside it
                client_thread->detach();
                clients.erase(it);
            }
        }
    }

    // Helper function to split a string into tokens
    std::vector<std::string> split_string(const std::string &str)
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

    std::string Server::process_command(const std::string &command)
    {
        std::cout << "Processing command: " << command << std::endl;

        if (command.empty())
        {
            return "Error: Empty command";
        }

        // Get command code (first character)
        char cmd_code = command[0];

        // Split the command into tokens
        std::vector<std::string> tokens = split_string(command);

        // Process based on command code
        switch (cmd_code)
        {
        case constants::CMD_PUT:
        {
            if (tokens.size() != 3)
            {
                return "Error: Put command requires exactly two arguments: p [key] [value]";
            }
            try
            {
                int key = std::stoi(tokens[1]);
                int value = std::stoi(tokens[2]);
                return "SUCCESS: Put command received - would store key " + std::to_string(key) +
                       " with value " + std::to_string(value);
            }
            catch (const std::exception &e)
            {
                return "Error: Invalid arguments for put command. Integers required.";
            }
        }

        case constants::CMD_GET:
        {
            if (tokens.size() != 2)
            {
                return "Error: Get command requires exactly one argument: g [key]";
            }
            try
            {
                int key = std::stoi(tokens[1]);
                return "SUCCESS: Get command received for key " + std::to_string(key);
            }
            catch (const std::exception &e)
            {
                return "Error: Invalid argument for get command. Integer required.";
            }
        }

        case constants::CMD_RANGE:
        {
            if (tokens.size() != 3)
            {
                return "Error: Range command requires exactly two arguments: r [start] [end]";
            }
            try
            {
                int start = std::stoi(tokens[1]);
                int end = std::stoi(tokens[2]);

                // Validate that start < end
                if (start >= end)
                {
                    return "Error: Range query requires start key to be less than end key";
                }

                return "SUCCESS: Range command received for keys " + std::to_string(start) +
                       " to " + std::to_string(end);
            }
            catch (const std::exception &e)
            {
                return "Error: Invalid arguments for range command. Integers required.";
            }
        }

        case constants::CMD_DELETE:
        {
            if (tokens.size() != 2)
            {
                return "Error: Delete command requires exactly one argument: d [key]";
            }
            try
            {
                int key = std::stoi(tokens[1]);
                return "SUCCESS: Delete command received for key " + std::to_string(key);
            }
            catch (const std::exception &e)
            {
                return "Error: Invalid argument for delete command. Integer required.";
            }
        }

        case constants::CMD_LOAD:
        {
            // Check for proper format: l "filepath"
            std::string filepath;
            size_t start_pos = command.find_first_of("\"'");

            if (start_pos == std::string::npos)
            {
                return "Error: Load command requires filepath in quotes: l \"filepath\"";
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

            return "SUCCESS: Load command received for file: " + filepath;
        }

        case constants::CMD_STATS:
        {
            if (tokens.size() > 1)
            {
                return "Error: Stats command takes no arguments: s";
            }
            return "SUCCESS: Stats command received. Would display tree statistics.";
        }

        case constants::CMD_HELP:
        {
            if (tokens.size() > 1)
            {
                return "Error: Help command takes no arguments: h";
            }
            return constants::HELP_TEXT;
        }

        case constants::CMD_EXIT[0]: // Note: CMD_EXIT is now "q" instead of "EXIT"
        {
            if (tokens.size() > 1)
            {
                return "Error: Quit command takes no arguments: q";
            }
            return "Disconnecting...";
        }

        default:
            return "Error: Unknown command. Type 'h' for help.";
        }
    }

} // namespace lsm