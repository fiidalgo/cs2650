#include "../include/server.h"
#include "../include/constants.h"

#include <iostream>
#include <string>
#include <signal.h>
#include <unistd.h>

// Global server instance for signal handling
lsm::Server *g_server = nullptr;

// Signal handler for clean shutdown
void signal_handler(int signal)
{
    if (g_server)
    {
        std::cout << "Caught signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

int main(int argc, char *argv[])
{
    // Parse command line arguments for port
    int port = lsm::constants::DEFAULT_PORT;

    if (argc > 1)
    {
        try
        {
            port = std::stoi(argv[1]);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid port number: " << argv[1] << std::endl;
            return 1;
        }
    }

    // Setup signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try
    {
        // Create and start server
        lsm::Server server(port);
        g_server = &server;

        std::cout << "Starting LSM-Tree server on port " << port << std::endl;
        server.start();

        // Wait for user input to stop server
        std::cout << "Server running. Press Enter to stop." << std::endl;
        std::cin.get();

        std::cout << "Stopping server..." << std::endl;
        server.stop();
        g_server = nullptr;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}