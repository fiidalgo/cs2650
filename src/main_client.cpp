#include "../include/client.h"
#include "../include/constants.h"

#include <iostream>
#include <string>
#include <signal.h>
#include <sstream>

// Global client instance for signal handling
lsm::Client *g_client = nullptr;

// Signal handler for clean shutdown
void signal_handler(int signal)
{
    if (g_client)
    {
        std::cout << "Caught signal " << signal << ", disconnecting..." << std::endl;
        g_client->disconnect();
        exit(0);
    }
}

// Utility function to process and display the server's response
void display_response(const std::string &command, const std::string &response)
{
    // Special case for stats command - it already has its own formatting
    if (command.length() > 0 && command[0] == 's')
    {
        std::cout << response << std::endl;
        return;
    }

    // For get command that returned a value
    if (command.length() > 0 && command[0] == 'g' && !response.empty())
    {
        std::cout << response << std::endl;
        return;
    }

    // For other commands with responses
    if (!response.empty())
    {
        std::cout << "Response: " << response << std::endl;
    }
    else
    {
        // Empty response could mean key not found for get command
        if (command.length() > 0 && command[0] == 'g')
        {
            std::cout << "Key not found" << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    // Parse command line arguments for host and port
    std::string host = lsm::constants::DEFAULT_HOST;
    int port = lsm::constants::DEFAULT_PORT;

    if (argc > 1)
    {
        host = argv[1];
    }

    if (argc > 2)
    {
        try
        {
            port = std::stoi(argv[2]);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid port number: " << argv[2] << std::endl;
            return 1;
        }
    }

    // Setup signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try
    {
        // Create client
        lsm::Client client(host, port);
        g_client = &client;

        // Connect to server
        std::cout << "Connecting to LSM-Tree server at " << host << ":" << port << std::endl;
        if (!client.connect())
        {
            std::cerr << "Failed to connect to server" << std::endl;
            return 1;
        }

        // Interactive command loop
        std::string command;
        std::cout << "Enter commands (type 'q' to quit):" << std::endl;

        while (true)
        {
            std::cout << "> ";
            std::getline(std::cin, command);

            if (command.empty())
            {
                continue;
            }

            if (command == lsm::constants::CMD_EXIT)
            {
                client.disconnect();
                break;
            }

            try
            {
                std::string response = client.send_command(command);
                display_response(command, response);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                if (!client.is_connected())
                {
                    std::cerr << "Lost connection to server" << std::endl;
                    break;
                }
            }
        }

        g_client = nullptr;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}