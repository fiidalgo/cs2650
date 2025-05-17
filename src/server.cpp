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
                std::cout << "Closing client socket: " << client.first << std::endl;
                shutdown(client.first, SHUT_RDWR); // Properly shutdown the socket
                close(client.first);
            }
        }

        // Wait for the connection handling thread to finish with a short timeout
        if (connection_thread && connection_thread->joinable())
        {
            std::cout << "Waiting for connection thread to finish..." << std::endl;

            // Use a detached thread to join with timeout
            std::thread join_thread([this]()
                                    {
                if (connection_thread->joinable()) {
                    connection_thread->join();
                } });

            // Wait briefly and detach
            join_thread.detach();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // IMPORTANT: Detach client threads instead of joining them
        // This prevents the server from hanging if clients are stuck
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto &client : clients)
            {
                std::cout << "Detaching client thread for socket: " << client.first << std::endl;
                if (client.second && client.second->joinable())
                {
                    client.second->detach();
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

        std::cout << "Sent welcome message to client " << client_socket << " (" << welcome_sent << " bytes)" << std::endl;

        while (running.load())
        {
            std::cout << "Waiting for command from client " << client_socket << "..." << std::endl;
            std::memset(buffer, 0, constants::BUFFER_SIZE);
            ssize_t bytes_read = recv(client_socket, buffer, constants::BUFFER_SIZE - 1, 0);

            if (bytes_read < 0)
            {
                if (errno == EINTR && running.load())
                {
                    // Interrupted system call, try again if still running
                    continue;
                }
                std::cerr << "Error receiving from client " << client_socket << ": " << strerror(errno) << std::endl;
                break;
            }

            if (bytes_read == 0)
            {
                std::cout << "Client " << client_socket << " closed connection" << std::endl;
                break;
            }

            std::cout << "Read " << bytes_read << " bytes from client " << client_socket << std::endl;

            // Add received data to the command buffer
            command_buffer.append(buffer, bytes_read);

            std::cout << "Command buffer now contains: '" << command_buffer << "'" << std::endl;

            // Process complete commands (delimited by \r\n)
            size_t delimiter_pos;
            while ((delimiter_pos = command_buffer.find(constants::CMD_DELIMITER)) != std::string::npos)
            {
                // Extract the command
                std::string command = command_buffer.substr(0, delimiter_pos);
                command_buffer.erase(0, delimiter_pos + strlen(constants::CMD_DELIMITER));

                std::cout << "Extracted command: '" << command << "', remaining buffer: '" << command_buffer << "'" << std::endl;

                // Check for exit command
                if (command == constants::CMD_EXIT)
                {
                    std::cout << "Client requested disconnect (socket: " << client_socket << ")" << std::endl;
                    goto cleanup;
                }

                // Log received command
                std::cout << "Received command from client " << client_socket << ": " << command << std::endl;

                // Special handling for bulk load commands, which can take a long time
                if (command.length() > 0 && command[0] == constants::CMD_LOAD)
                {
                    // Send an initial acknowledgment
                    std::string ack_msg = "Processing load command, this may take some time..." + std::string(constants::CMD_DELIMITER);
                    ssize_t ack_sent = send(client_socket, ack_msg.c_str(), ack_msg.length(), 0);
                    if (ack_sent < 0)
                    {
                        std::cerr << "Error sending load acknowledgment: " << strerror(errno) << std::endl;
                    }
                    else
                    {
                        std::cout << "Sent load acknowledgment to client " << client_socket << " (" << ack_sent << " bytes)" << std::endl;
                    }

                    // Process the bulk load command
                    std::cout << "Processing bulk load command: '" << command << "'" << std::endl;
                    std::string response = this->process_command(command);

                    // Format the response carefully for bulk load completion
                    std::cout << "Command processed, response: '" << response << "'" << std::endl;

                    // Ensure we have some response even for empty results
                    if (response.empty())
                    {
                        response = "File loaded successfully";
                        std::cout << "Empty response replaced with: '" << response << "'" << std::endl;
                    }

                    // Add an additional newline and delimiter for bulk load operations
                    if (!response.empty() && response.back() != '\n')
                    {
                        response += '\n';
                    }
                    response += constants::CMD_DELIMITER;

                    // Send the response with careful error handling
                    std::cout << "Sending bulk load response to client " << client_socket << std::endl;
                    std::cout << "Response length: " << response.length() << " bytes" << std::endl;

                    // Send in chunks with careful error handling
                    const size_t CHUNK_SIZE = 4096;
                    size_t bytes_sent = 0;
                    size_t total_bytes = response.length();

                    while (bytes_sent < total_bytes)
                    {
                        size_t chunk_size = std::min(CHUNK_SIZE, total_bytes - bytes_sent);
                        ssize_t sent = send(client_socket, response.c_str() + bytes_sent, chunk_size, 0);

                        if (sent < 0)
                        {
                            if (errno == EINTR && running.load())
                            {
                                // Interrupted, try again
                                continue;
                            }
                            std::cerr << "Error sending bulk load response to client " << client_socket << ": "
                                      << strerror(errno) << std::endl;
                            goto cleanup;
                        }

                        bytes_sent += sent;
                        std::cout << "Sent chunk of " << sent << " bytes, total sent: " << bytes_sent << "/" << total_bytes << std::endl;
                    }

                    std::cout << "Bulk load response fully sent (" << bytes_sent << " bytes total)" << std::endl;

                    // Check if client is still connected after bulk loading
                    struct timeval tv;
                    tv.tv_sec = 0;
                    tv.tv_usec = 0;

                    fd_set read_fds;
                    FD_ZERO(&read_fds);
                    FD_SET(client_socket, &read_fds);

                    int select_result = select(client_socket + 1, &read_fds, NULL, NULL, &tv);
                    if (select_result > 0)
                    {
                        // Socket has data or has been closed
                        char check_buffer[1];
                        ssize_t check_bytes = recv(client_socket, check_buffer, 1, MSG_PEEK);
                        if (check_bytes == 0)
                        {
                            std::cout << "Client " << client_socket << " disconnected after bulk load" << std::endl;
                            goto cleanup;
                        }
                    }
                }
                else
                {
                    // Regular (non-bulk-load) command processing
                    std::cout << "Processing command: '" << command << "'" << std::endl;
                    std::string response = this->process_command(command);
                    std::cout << "Command processed, response: '" << response << "'" << std::endl;

                    // Ensure we have some response even for empty results
                    if (response.empty())
                    {
                        if (command[0] == constants::CMD_GET)
                        {
                            response = "Key not found";
                        }
                        else if (command[0] == constants::CMD_RANGE)
                        {
                            response = "No results in range";
                        }
                        else
                        {
                            response = "Operation completed";
                        }
                        std::cout << "Empty response replaced with: '" << response << "'" << std::endl;
                    }

                    // Add delimiter to response if it doesn't have one
                    if (response.find(constants::CMD_DELIMITER) == std::string::npos)
                    {
                        response += constants::CMD_DELIMITER;
                    }

                    // Send the response
                    std::cout << "Sending response to client " << client_socket << " for command: " << command << std::endl;
                    std::cout << "Response length: " << response.length() << " bytes, content: '" << response << "'" << std::endl;

                    // Send in smaller chunks to prevent issues with large responses
                    const size_t CHUNK_SIZE = 4096;
                    size_t bytes_sent = 0;
                    size_t total_bytes = response.length();

                    while (bytes_sent < total_bytes)
                    {
                        size_t chunk_size = std::min(CHUNK_SIZE, total_bytes - bytes_sent);
                        ssize_t sent = send(client_socket, response.c_str() + bytes_sent, chunk_size, 0);

                        if (sent < 0)
                        {
                            std::cerr << "Error sending response to client " << client_socket << ": "
                                      << strerror(errno) << std::endl;
                            goto cleanup;
                        }

                        bytes_sent += sent;
                        std::cout << "Sent chunk of " << sent << " bytes, total sent: " << bytes_sent << "/" << total_bytes << std::endl;
                    }

                    std::cout << "Response sent successfully (" << bytes_sent << " bytes total)" << std::endl;
                }
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
            
            auto tokens = split_string(command);
            if (tokens.size() > 1)
            {
                return "Error: Help command takes no arguments: h";
            }
            return constants::HELP_TEXT;
        }

        case constants::CMD_EXIT[0]:
        {
            auto tokens = split_string(command);
            if (tokens.size() > 1)
            {
                return "Error: Quit command takes no arguments: q";
            }
            return "Disconnecting...";
        }

        case constants::CMD_LOAD:
        {
            try
            {
                std::cout << "Processing LOAD command - this may take a while..." << std::endl;

                // Extract filepath from command
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
                std::cout << "Loading file: " << filepath << std::endl;

                // Forward to LSM adapter for actual processing
                std::string result = LSMAdapter::get_instance().process_command(command);
                std::cout << "Load command complete for " << filepath << std::endl;

                // Return a more detailed success message
                if (result.empty() || result.find("successfully") != std::string::npos)
                {
                    std::stringstream ss;
                    ss << "File loaded successfully: " << filepath << std::endl;
                    ss << "Loaded data is now available for queries.";
                    return ss.str();
                }
                return result;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Load command error: " << e.what() << std::endl;
                return std::string("Error loading file: ") + e.what();
            }
        }

        case constants::CMD_STATS:
        {
            try
            {
                // Log that we're getting stats
                std::cout << "Generating stats - this may take a moment..." << std::endl;

                // Get stats directly (bypass LSM adapter to debug)
                std::stringstream ss;
                ss << "LSM-Tree Statistics Summary:" << std::endl;
                ss << "==========================" << std::endl;
                ss << "Buffer Size: " << constants::BUFFER_SIZE_BYTES << " bytes" << std::endl;
                ss << "Size Ratio: " << constants::SIZE_RATIO << std::endl;
                ss << "Level Count: " << constants::INITIAL_MAX_LEVEL << std::endl;
                ss << "==========================" << std::endl;

                // Try to get detailed stats from the LSM adapter
                try
                {
                    std::string lsm_stats = LSMAdapter::get_instance().process_command(command);
                    if (!lsm_stats.empty())
                    {
                        ss << "Detailed stats:" << std::endl
                           << lsm_stats;
                    }
                    else
                    {
                        ss << "No detailed stats available - tree may be empty" << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
                    ss << "Error getting detailed stats: " << e.what() << std::endl;
                }

                std::string result = ss.str();
                std::cout << "Generated stats successfully (" << result.length() << " bytes)" << std::endl;
                return result;
            }
            catch (const std::exception &e)
            {
                return std::string("Error generating stats: ") + e.what();
            }
        }

        default:
            // Use the LSM adapter to process all other commands
            try
            {
                // Log that we're sending the command to the LSM adapter
                std::cout << "Forwarding command to LSM adapter: " << command << std::endl;
                std::string result = LSMAdapter::get_instance().process_command(command);
                std::cout << "LSM adapter processed command, result length: " << result.length() << " bytes" << std::endl;
                return result;
            }
            catch (const std::exception &e)
            {
                return std::string("Error: ") + e.what();
            }
        }
    }

}