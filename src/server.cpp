#include "../include/server.h"
#include "../include/lsm_adapter.h"

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

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

        // Initialize the LSM tree adapter before accepting connections
        std::cout << "Pre-initializing LSM tree..." << std::endl;
        LSMAdapter::get_instance(); // This will trigger the adapter's constructor and initialize the tree
        std::cout << "LSM tree ready" << std::endl;

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

        std::cout << "Stopping server..." << std::endl;
        running.store(false);

        // Close the server socket to unblock the accept() call
        if (server_socket != -1)
        {
            close(server_socket);
            server_socket = -1;
        }

        // First, close all client sockets to unblock recv() calls
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto &client : clients)
            {
                close(client.first);
            }
        }

        // Wait for the connection handling thread to finish
        if (connection_thread && connection_thread->joinable())
        {
            connection_thread->join();
        }

        // Now wait for all client threads to finish
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto &client : clients)
            {
                if (client.second && client.second->joinable())
                {
                    client.second->join();
                }
            }
            clients.clear();
        }

        // Make sure the LSM tree is properly shut down
        std::cout << "Shutting down LSM adapter..." << std::endl;
        LSMAdapter::get_instance().shutdown();

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

        // Send a welcome message confirming the LSM tree is ready
        std::string welcome_msg = "LSM-Tree ready and waiting for commands" + std::string(constants::CMD_DELIMITER);
        ssize_t welcome_sent = send(client_socket, welcome_msg.c_str(), welcome_msg.length(), 0);
        if (welcome_sent < 0)
        {
            std::cerr << "Error sending welcome message: " << strerror(errno) << std::endl;
            goto cleanup;
        }

        while (running.load())
        {
            std::memset(buffer, 0, constants::BUFFER_SIZE);
            ssize_t bytes_read = recv(client_socket, buffer, constants::BUFFER_SIZE - 1, 0);

            if (bytes_read < 0)
            {
                std::cerr << "Error receiving from client " << client_socket << ": " << strerror(errno) << std::endl;
                break;
            }

            if (bytes_read == 0)
            {
                std::cout << "Client " << client_socket << " closed connection" << std::endl;
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

                // Log received command
                std::cout << "Received command from client " << client_socket << ": " << command << std::endl;

                // Submit command to thread pool
                auto future = thread_pool->enqueue([this, command, client_socket]()
                                                   {
                    std::string response = this->process_command(command);
                    
                    // Ensure we have some response even for empty results
                    if (response.empty() && command[0] == 'g') {
                        response = "Key not found";
                    } else if (response.empty() && command[0] == 'r') {
                        response = "No results in range";
                    } else if (response.empty()) {
                        response = "Operation completed";
                    }
                    
                    response += constants::CMD_DELIMITER;  // Add delimiter to response
                    
                    // For large responses, send in chunks to avoid blocking
                    const size_t chunk_size = 4096;
                    size_t bytes_sent = 0;
                    const char* resp_data = response.c_str();
                    size_t remaining = response.length();
                    
                    while (remaining > 0 && running.load()) {
                        size_t to_send = std::min(chunk_size, remaining);
                        ssize_t sent = send(client_socket, resp_data + bytes_sent, to_send, 0);
                        
                        if (sent < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // Socket buffer full, wait a bit and retry
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                continue;
                            }
                            // Real error
                            std::cerr << "Error sending response to client " << client_socket << ": " << strerror(errno) << std::endl;
                            break;
                        }
                        
                        if (sent == 0) {
                            // Connection closed
                            std::cerr << "Connection closed while sending response to client " << client_socket << std::endl;
                            break;
                        }
                        
                        bytes_sent += sent;
                        remaining -= sent;
                    }
                    
                    // Log completion for debugging
                    std::cout << "Command processed and response sent to client " << client_socket << ": " << command.substr(0, 20) 
                              << (command.length() > 20 ? "..." : "") << std::endl;
                                    
                    return true; });
            }
        }

    cleanup:
        std::cout << "Cleaning up client connection: " << client_socket << std::endl;
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
        std::cout << "Client " << client_socket << " disconnected" << std::endl;
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

        // Process based on command code
        switch (cmd_code)
        {
        case constants::CMD_HELP:
        {
            // Special case for help command
            auto tokens = split_string(command);
            if (tokens.size() > 1)
            {
                return "Error: Help command takes no arguments: h";
            }
            return constants::HELP_TEXT;
        }

        case constants::CMD_EXIT[0]: // Note: CMD_EXIT is now "q" instead of "EXIT"
        {
            auto tokens = split_string(command);
            if (tokens.size() > 1)
            {
                return "Error: Quit command takes no arguments: q";
            }
            return "Disconnecting...";
        }

        default:
            // Use the LSM adapter to process all other commands
            try
            {
                return LSMAdapter::get_instance().process_command(command);
            }
            catch (const std::exception &e)
            {
                return std::string("Error: ") + e.what();
            }
        }
    }

} // namespace lsm