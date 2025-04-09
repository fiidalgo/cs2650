#include "server/server.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

namespace server
{

    Server::Server(const std::string &data_dir, const std::string &impl_type)
        : running_(false), data_dir_(data_dir), impl_type_(impl_type), db_()
    {
        // Create the data directory if it doesn't exist
        std::filesystem::path dir_path = data_dir;
        if (impl_type == "naive")
        {
            dir_path /= "naive";
        }
        else if (impl_type == "compaction")
        {
            dir_path /= "compaction";
        }
        else if (impl_type == "bloom")
        {
            dir_path /= "bloom";
        }
        else if (impl_type == "fence")
        {
            dir_path /= "fence";
        }
        else if (impl_type == "concurrency")
        {
            dir_path /= "concurrency";
        }
        else
        {
            // Default to naive
            dir_path /= "naive";
            impl_type_ = "naive";
        }

        if (!std::filesystem::exists(dir_path))
        {
            std::filesystem::create_directories(dir_path);
        }

        // Initialize the LSM-Tree
        // Currently only the naive implementation is supported
        lsm_tree_ = std::make_unique<naive::LSMTree>(dir_path.string());

        std::cout << "Server started with " << impl_type << " implementation.\n";
        std::cout << "Data directory: " << data_dir << "\n";

        parser_ = std::make_unique<DSLParser>();
    }

    void Server::run()
    {
        std::string command_str;
        std::string response;

        // Print welcome message and help information
        std::cout << "Welcome to LSM-Tree Database\n";
        std::cout << "Type 'h' for help or 'q' to exit\n\n";

        // Main command loop
        while (true)
        {
            // Print prompt
            std::cout << "lsmdb> ";

            // Get command
            if (!std::getline(std::cin, command_str) || command_str == "q" || command_str == "exit")
            {
                break;
            }

            // Parse and execute command
            if (!command_str.empty())
            {
                response = execute_command(command_str);
                std::cout << response << std::endl;
            }
        }

        std::cout << "Server shutting down.\n";
    }

    void Server::run_socket_server(int port)
    {
        // Create socket
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            std::cerr << "Error creating socket" << std::endl;
            return;
        }

        // Set socket options
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            std::cerr << "Error setting socket options" << std::endl;
            close(server_fd);
            return;
        }

        // Bind socket to address
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            std::cerr << "Error binding socket to port " << port << std::endl;
            close(server_fd);
            return;
        }

        // Listen for connections
        if (listen(server_fd, 3) < 0)
        {
            std::cerr << "Error listening for connections" << std::endl;
            close(server_fd);
            return;
        }

        std::cout << "Server started and listening on port " << port << std::endl;
        running_ = true;

        // Buffer for receiving commands
        char buffer[1024];

        // Accept and handle client connections
        while (running_)
        {
            std::cout << "Waiting for client connections..." << std::endl;

            // Accept a new connection
            int addrlen = sizeof(address);
            int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

            if (client_fd < 0)
            {
                if (running_)
                {
                    std::cerr << "Error accepting connection" << std::endl;
                }
                continue;
            }

            std::cout << "Client connected" << std::endl;

            // Handle client requests
            bool client_connected = true;
            while (client_connected && running_)
            {
                // Receive command from client
                memset(buffer, 0, sizeof(buffer));
                int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes_read <= 0)
                {
                    // Client disconnected or error
                    client_connected = false;
                    continue;
                }

                // Null-terminate the command
                buffer[bytes_read] = '\0';

                // Process the command
                std::string command_str(buffer);
                std::string response;

                // Check for exit command
                if (command_str == "q" || command_str == "exit")
                {
                    response = "Server closing connection";
                    send(client_fd, response.c_str(), response.length(), 0);
                    client_connected = false;
                    continue;
                }

                // Execute the command
                std::cout << "Received command: " << command_str << std::endl;
                response = execute_command(command_str);

                // Send response back to client
                send(client_fd, response.c_str(), response.length(), 0);
            }

            // Close connection
            close(client_fd);
            std::cout << "Client disconnected" << std::endl;
        }

        // Close server socket
        close(server_fd);
        std::cout << "Server shut down" << std::endl;
    }

    std::string Server::execute_command(const std::string &command_str)
    {
        Command cmd = parser_->parse(command_str);

        try
        {
            switch (cmd.type)
            {
            case CommandType::PUT:
                if (cmd.key1.has_value() && cmd.value.has_value())
                {
                    db_.put(cmd.key1.value(), cmd.value.value());
                    return "OK";
                }
                return "Error: PUT command requires key and value";

            case CommandType::GET:
                if (cmd.key1.has_value())
                {
                    auto value = db_.get(cmd.key1.value());
                    if (value.has_value())
                    {
                        return std::to_string(value.value());
                    }
                    return "Key not found";
                }
                return "Error: GET command requires a key";

            case CommandType::DELETE:
                if (cmd.key1.has_value())
                {
                    bool removed = db_.remove(cmd.key1.value());
                    return removed ? "OK" : "Key not found";
                }
                return "Error: DELETE command requires a key";

            case CommandType::RANGE:
                if (cmd.key1.has_value() && cmd.key2.has_value())
                {
                    auto range_result = db_.range(cmd.key1.value(), cmd.key2.value());
                    if (range_result.empty())
                    {
                        return "No keys found in range";
                    }

                    std::string result = "Range results:";
                    for (const auto &[key, value] : range_result)
                    {
                        result += "\n" + std::to_string(key) + ": " + std::to_string(value);
                    }
                    return result;
                }
                return "Error: RANGE command requires start_key and end_key";

            case CommandType::STATS:
                return get_stats();

            case CommandType::HELP:
                return parser_->get_help();

            case CommandType::EXIT:
                return "Goodbye!";

            default:
                return "Unknown command. Type 'h' for help.";
            }
        }
        catch (const std::exception &e)
        {
            return std::string("Error: ") + e.what();
        }
    }

    std::string Server::get_stats()
    {
        std::string stats = "Database Statistics:\n";
        stats += "----------------\n";
        stats += "Implementation type: " + impl_type_ + "\n";

        // Get entry count
        int count = 0;
        db_.for_each([&count](int /* key */, const std::optional<int> &value)
                     {
            if (value.has_value()) {
                count++;
            } });

        stats += "Total entries: " + std::to_string(count) + "\n";

        // Future: Add more statistics like memory usage, SSTables count, etc.

        return stats;
    }

    bool Server::is_running() const
    {
        return running_;
    }

    void Server::stop()
    {
        running_ = false;
        std::cout << "Server stopped.\n";
    }

} // namespace server