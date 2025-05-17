#include "../include/client.h"
#include "../include/constants.h"

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>
#include <fcntl.h>

namespace lsm
{

    Client::Client(const std::string &host, int port)
        : host(host),
          port(port),
          client_socket(-1),
          connected(false)
    {
    }

    Client::~Client()
    {
        disconnect();
    }

    bool Client::connect()
    {
        if (connected.load())
        {
            return true;
        }

        // Create socket
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1)
        {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Resolve host name
        struct hostent *host_entry = gethostbyname(host.c_str());
        if (!host_entry)
        {
            std::cerr << "Failed to resolve hostname: " << host << std::endl;
            close(client_socket);
            client_socket = -1;
            return false;
        }

        // Setup server address
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        std::memcpy(&server_addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);

        // Connect to server
        if (::connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            std::cerr << "Connection failed: " << strerror(errno) << std::endl;
            close(client_socket);
            client_socket = -1;
            return false;
        }

        // Clear any existing responses
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            while (!response_queue.empty())
            {
                response_queue.pop();
            }
        }

        connected.store(true);
        std::cout << "Connected to server at " << host << ":" << port << std::endl;

        // Start the receiving thread
        receive_thread = std::make_unique<std::thread>(&Client::receive_responses, this);

        // Display help menu on successful connection
        std::cout << constants::HELP_TEXT << std::endl;

        // Wait for the welcome message from the server
        char welcome_buffer[constants::BUFFER_SIZE];
        ssize_t bytes_read = recv(client_socket, welcome_buffer, constants::BUFFER_SIZE - 1, 0);

        if (bytes_read > 0)
        {
            welcome_buffer[bytes_read] = '\0';
            std::string welcome_message(welcome_buffer);

            // Remove delimiter if present
            size_t delimiter_pos = welcome_message.find(constants::CMD_DELIMITER);
            if (delimiter_pos != std::string::npos)
            {
                welcome_message = welcome_message.substr(0, delimiter_pos);
            }

            std::cout << "Server: " << welcome_message << std::endl;
        }

        return true;
    }

    void Client::disconnect()
    {
        if (!connected.load())
        {
            return;
        }

        connected.store(false);

        // Send exit command if possible
        try
        {
            send_command(constants::CMD_EXIT);
        }
        catch (...)
        {
            // Ignore errors during shutdown
        }

        // Close socket to interrupt the receive thread
        if (client_socket != -1)
        {
            close(client_socket);
            client_socket = -1;
        }

        // Wait for receive thread to finish
        if (receive_thread && receive_thread->joinable())
        {
            receive_thread->join();
        }

        std::cout << "Disconnected from server" << std::endl;
    }

    std::string Client::send_command(const std::string &command)
    {
        if (!connected.load())
        {
            throw std::runtime_error("Not connected to server");
        }

        // Exit command doesn't get a response
        if (command == constants::CMD_EXIT)
        {
            std::string full_command = command + constants::CMD_DELIMITER;
            send(client_socket, full_command.c_str(), full_command.length(), 0);
            return "";
        }

        // Add delimiter to the command
        std::string full_command = command + constants::CMD_DELIMITER;

        // Send the command
        if (send(client_socket, full_command.c_str(), full_command.length(), 0) < 0)
        {
            throw std::runtime_error("Failed to send command: " + std::string(strerror(errno)));
        }

        // Log that command was sent
        std::cout << "Sent command: " << command << std::endl;

        // Check if this is a load command for a large file
        bool is_large_file_load = false;
        if (command.length() > 1 && command[0] == constants::CMD_LOAD)
        {
            // This is a load command, check if it's for a large test file
            if (command.find("10gb") != std::string::npos ||
                command.find("test_data") != std::string::npos)
            {
                is_large_file_load = true;
                std::cout << "Loading a large file - timeout extended to 2 hours" << std::endl;
            }
        }

        // Wait for a response with appropriate timeout
        std::string response;
        char buffer[4096];

        auto start_time = std::chrono::steady_clock::now();
        auto timeout_duration = is_large_file_load ? std::chrono::hours(2) : std::chrono::minutes(2);

        // Print the timeout details for debugging
        std::cout << "Waiting for server response with "
                  << (is_large_file_load ? "extended timeout (2 hours)" : "standard timeout (2 minutes)")
                  << std::endl;

        // Set socket to non-blocking mode to prevent hangs
        int flags = fcntl(client_socket, F_GETFL, 0);
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

        // Track partial responses and progress
        auto last_activity = std::chrono::steady_clock::now();
        bool has_received_data = false;
        size_t total_bytes_received = 0;

        while (true)
        {
            // Check if we've exceeded timeout, but if we've received data recently, give extra time
            auto current_time = std::chrono::steady_clock::now();
            auto time_since_start = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
            auto time_since_activity = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_activity);

            // Only time out if we've exceeded total timeout or have been inactive for over a minute
            bool timeout_exceeded = time_since_start > timeout_duration;
            bool inactive_too_long = has_received_data && time_since_activity > std::chrono::minutes(1);

            if (timeout_exceeded && (!has_received_data || inactive_too_long))
            {
                // Reset socket to blocking mode before returning
                fcntl(client_socket, F_SETFL, flags);

                // Form a helpful error message
                std::string error_msg = "Timeout waiting for server response after " +
                                        std::to_string(time_since_start.count()) +
                                        " seconds";
                if (has_received_data)
                {
                    error_msg += " (received " + std::to_string(total_bytes_received) +
                                 " bytes, but response was incomplete)";
                }
                throw std::runtime_error(error_msg);
            }

            // Use select to wait for data with a short timeout
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);

            struct timeval tv;
            tv.tv_sec = 0;       
            tv.tv_usec = 500000; 

            int select_result = select(client_socket + 1, &readfds, NULL, NULL, &tv);

            if (select_result < 0)
            {
                if (errno == EINTR)
                {
                    // Interrupted, try again
                    continue;
                }
                // Reset socket to blocking mode before returning
                fcntl(client_socket, F_SETFL, flags);
                throw std::runtime_error("Error waiting for server response: " + std::string(strerror(errno)));
            }

            if (select_result == 0)
            {
                // Timeout on select
                if (has_received_data && response.find("Processing load command") != std::string::npos)
                {
                    // Found processing message, print progress dots
                    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_activity).count();
                    if (seconds > 0 && seconds % 5 == 0)
                    {
                        std::cout << "." << std::flush;
                    }
                }
                continue;
            }

            if (FD_ISSET(client_socket, &readfds))
            {
                // Data is available, try to read it
                std::memset(buffer, 0, sizeof(buffer));
                ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

                if (bytes_read < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // No data available at the moment, continue waiting
                        continue;
                    }
                    // Reset socket to blocking mode before returning
                    fcntl(client_socket, F_SETFL, flags);
                    throw std::runtime_error("Error receiving response: " + std::string(strerror(errno)));
                }

                if (bytes_read == 0)
                {
                    // Connection closed by server
                    fcntl(client_socket, F_SETFL, flags);
                    throw std::runtime_error("Connection closed by server");
                }

                // Update activity time and flags
                last_activity = std::chrono::steady_clock::now();
                has_received_data = true;
                total_bytes_received += bytes_read;

                buffer[bytes_read] = '\0';
                response.append(buffer, bytes_read);

                // log partial response
                std::cout << "Received partial response (" << bytes_read << " bytes): "
                          << std::string(buffer, std::min(bytes_read, static_cast<ssize_t>(20)))
                          << (bytes_read > 20 ? "..." : "") << std::endl;

                // Check if this is an acknowledgment for a load command
                if (command[0] == constants::CMD_LOAD &&
                    response.find("Processing load command") != std::string::npos &&
                    response.find(constants::CMD_DELIMITER) != std::string::npos)
                {
                    // This is just an acknowledgment, continue waiting for the actual response
                    std::cout << "Received load acknowledgment, waiting for final response..." << std::endl;

                    // Clear the response buffer and continue waiting
                    response.clear();
                    continue;
                }

                // Check if we've received a complete response
                size_t delimiter_pos = response.find(constants::CMD_DELIMITER);
                if (delimiter_pos != std::string::npos)
                {
                    // Complete response received, extract it
                    std::string complete_response = response.substr(0, delimiter_pos);

                    // Log completion
                    std::cout << "Complete response received (" << complete_response.length()
                              << " bytes) after "
                              << std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::steady_clock::now() - start_time)
                                     .count()
                              << " seconds" << std::endl;

                    // Reset socket to blocking mode before returning
                    fcntl(client_socket, F_SETFL, flags);
                    return complete_response;
                }
            }
        }
    }

    bool Client::is_connected() const
    {
        return connected.load();
    }

    void Client::set_response_callback(std::function<void(const std::string &)> callback)
    {
        response_callback = callback;
    }

    void Client::receive_responses()
    {
        // This thread is now only used for asynchronous notifs
        char buffer[constants::BUFFER_SIZE];

        while (connected.load())
        {
            std::memset(buffer, 0, constants::BUFFER_SIZE);

            // Use select for non-blocking reads
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int select_result = select(client_socket + 1, &readfds, NULL, NULL, &timeout);

            // If error or timeout, just continue the loop
            if (select_result <= 0)
            {
                continue;
            }

            // Skip receiving data in this thread. The main send_command function handles responses
            // We keep this thread only to check if the connection is alive
            if (FD_ISSET(client_socket, &readfds) && connected.load())
            {
                // check if the socket is still connected
                char peek_buffer[1];
                ssize_t peek_result = recv(client_socket, peek_buffer, 1, MSG_PEEK | MSG_DONTWAIT);

                if (peek_result < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    // Real error on the socket
                    std::cerr << "Socket error detected: " << strerror(errno) << std::endl;
                    connected.store(false);
                    break;
                }

                if (peek_result == 0)
                {
                    // Connection closed by server
                    std::cerr << "Server closed connection" << std::endl;
                    connected.store(false);
                    break;
                }

                // Socket is still active, continue monitoring
            }
        }
    }

}