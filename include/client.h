#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace lsm
{

    class Client
    {
    public:
        Client(const std::string &host = "127.0.0.1", int port = 9090);
        ~Client();

        // Deleted copy/move constructors and assignment operators
        Client(const Client &) = delete;
        Client &operator=(const Client &) = delete;
        Client(Client &&) = delete;
        Client &operator=(Client &&) = delete;

        // Connect to the server
        bool connect();

        // Disconnect from the server
        void disconnect();

        // Send a command to the server and return the response
        std::string send_command(const std::string &command);

        // Check if connected to server
        bool is_connected() const;

        // Set a callback for response handling
        void set_response_callback(std::function<void(const std::string &)> callback);

    private:
        // Thread for receiving responses from the server
        void receive_responses();

        std::string host;
        int port;
        int client_socket;
        std::atomic<bool> connected;

        // Thread for receiving responses
        std::unique_ptr<std::thread> receive_thread;

        // Response callback
        std::function<void(const std::string &)> response_callback;

        // Response queue for synchronous operations
        std::queue<std::string> response_queue;
        std::mutex queue_mutex;
        std::condition_variable queue_condition;
    };

} // namespace lsm

#endif // CLIENT_H