#include "../include/server.h"
#include "../include/constants.h"
#include "../include/lsm_adapter.h"

#include <iostream>
#include <string>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <chrono>
#include <thread>

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

// Function to read environment variable with default fallback
template <typename T>
T get_env_var(const std::string &name, T default_value)
{
    const char *env_var = std::getenv(name.c_str());
    if (env_var == nullptr)
    {
        return default_value;
    }

    try
    {
        if constexpr (std::is_same_v<T, int>)
        {
            return std::stoi(env_var);
        }
        else if constexpr (std::is_same_v<T, size_t>)
        {
            return static_cast<size_t>(std::stoull(env_var));
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return std::string(env_var);
        }
        else
        {
            return default_value;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Warning: Failed to parse environment variable " << name << ": " << e.what() << std::endl;
        return default_value;
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
        // Pre-initialize the LSM adapter to ensure it's ready before accepting connections
        std::cout << "Initializing LSM tree adapter..." << std::endl;
        auto &adapter = lsm::LSMAdapter::get_instance();

        // Display current configuration
        size_t buffer_size = get_env_var<size_t>("LSMTREE_BUFFER_SIZE", lsm::constants::BUFFER_SIZE_BYTES);
        int size_ratio = get_env_var<int>("LSMTREE_SIZE_RATIO", lsm::constants::SIZE_RATIO);
        int thread_count = get_env_var<int>("LSMTREE_THREAD_COUNT", lsm::constants::default_thread_count());

        std::cout << "LSM Tree Configuration:" << std::endl;
        std::cout << "  Buffer Size: " << buffer_size << " bytes" << std::endl;
        std::cout << "  Size Ratio: " << size_ratio << std::endl;
        std::cout << "  Thread Count: " << thread_count << std::endl;

        // Create and start server
        lsm::Server server(port);
        g_server = &server;

        std::cout << "Starting LSM-Tree server on port " << port << std::endl;
        server.start();

        // Wait for user input to stop server or run indefinitely
        if (isatty(fileno(stdin)))
        {
            // Interactive mode
            std::cout << "Server running. Press Enter to stop." << std::endl;
            std::cin.get();

            std::cout << "Stopping server..." << std::endl;
            server.stop();
        }
        else
        {
            // Non-interactive mode (e.g., running as a daemon)
            std::cout << "Server running in non-interactive mode. Send SIGINT to stop." << std::endl;
            while (g_server != nullptr)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        g_server = nullptr;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}