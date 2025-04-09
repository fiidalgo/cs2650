#include <iostream>
#include <string>
#include <filesystem>
#include "server/server.h"
#include <cstdlib>

// Placeholder for the server main function
// This will be expanded later

int main(int argc, char *argv[])
{
    // Parse command line arguments
    std::string data_dir = "./data";
    std::string impl_type = "naive";
    bool socket_mode = false;
    int port = 9090;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--data-dir" && i + 1 < argc)
        {
            data_dir = argv[++i];
        }
        else if (arg == "--impl" && i + 1 < argc)
        {
            impl_type = argv[++i];
        }
        else if (arg == "--socket")
        {
            socket_mode = true;
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            port = std::atoi(argv[++i]);
        }
        else if (arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --data-dir DIR   Set the data directory (default: ./data)\n";
            std::cout << "  --impl TYPE      Set the implementation type (default: naive)\n";
            std::cout << "  --socket         Run in socket server mode\n";
            std::cout << "  --port PORT      Set the server port (default: 9090)\n";
            std::cout << "  --help           Show this help message and exit\n";
            return 0;
        }
    }

    // Check if data directory exists, create if not
    if (!std::filesystem::exists(data_dir))
    {
        std::cout << "Creating data directory: " << data_dir << std::endl;
        std::filesystem::create_directories(data_dir);
    }

    try
    {
        // Create server instance
        server::Server server(data_dir, impl_type);

        if (socket_mode)
        {
            std::cout << "Starting socket server on port " << port << "...\n";
            server.run_socket_server(port);
        }
        else
        {
            // Run the server in console mode
            std::cout << "Starting console server...\n";
            server.run();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}