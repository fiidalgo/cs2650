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

        connected.store(true);
        std::cout << "Connected to server at " << host << ":" << port << std::endl;

        // Start the receiving thread
        receive_thread = std::make_unique<std::thread>(&Client::receive_responses, this);

        // Display help menu on successful connection
        std::cout << constants::HELP_TEXT << std::endl;

        // Wait a moment for the welcome message
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check for and display welcome message
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!response_queue.empty())
            {
                std::string welcome = response_queue.front();
                response_queue.pop();
                std::cout << "Server: " << welcome << std::endl;
            }
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

        // Add delimiter to the command
        std::string full_command = command + constants::CMD_DELIMITER;

        // Send the command
        if (send(client_socket, full_command.c_str(), full_command.length(), 0) < 0)
        {
            throw std::runtime_error("Failed to send command: " + std::string(strerror(errno)));
        }

        // Exit command doesn't get a response
        if (command == constants::CMD_EXIT)
        {
            return "";
        }

        // Wait for response
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            using namespace std::chrono_literals;
            if (!queue_condition.wait_for(lock, 10s, [this]
                                          { return !response_queue.empty(); }))
            {
                throw std::runtime_error("Timeout waiting for server response");
            }

            std::string response = response_queue.front();
            response_queue.pop();
            return response;
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
        char buffer[constants::BUFFER_SIZE];
        std::string response_buffer;

        while (connected.load())
        {
            std::memset(buffer, 0, constants::BUFFER_SIZE);
            ssize_t bytes_read = recv(client_socket, buffer, constants::BUFFER_SIZE - 1, 0);

            if (bytes_read <= 0)
            {
                // Connection closed or error
                connected.store(false);
                break;
            }

            // Add received data to the response buffer
            response_buffer.append(buffer, bytes_read);

            // Process complete responses (delimited by \r\n)
            size_t delimiter_pos;
            while ((delimiter_pos = response_buffer.find(constants::CMD_DELIMITER)) != std::string::npos)
            {
                // Extract the response
                std::string response = response_buffer.substr(0, delimiter_pos);
                response_buffer.erase(0, delimiter_pos + strlen(constants::CMD_DELIMITER));

                // Add to response queue for synchronous operations
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    response_queue.push(response);
                }
                queue_condition.notify_one();

                // Call the callback if set
                if (response_callback)
                {
                    response_callback(response);
                }
            }
        }
    }

} // namespace lsm