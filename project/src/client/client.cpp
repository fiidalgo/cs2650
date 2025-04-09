#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "server/dsl_parser.h"

using namespace server;

// Helper function to print the help message
void print_help()
{
  std::cout << R"(
LSM-Tree Database Client
-----------------------
Commands:
  p <key> <value>   - Insert or update a key-value pair
  g <key>           - Retrieve the value for a key
  d <key>           - Delete a key-value pair
  r <start> <end>   - Get all key-value pairs in range [start, end)
  s                 - Show database statistics
  h                 - Show this help information
  q                 - Exit the client

Example:
  p 1 100           - Store value 100 under key 1
  g 1               - Retrieve the value for key 1
  r 1 5             - Get all key-value pairs with keys from 1 to 4
)" << std::endl;
}

#define SERVER_PORT 9090
#define BUFFER_SIZE 1024

int connect_to_server()
{
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "Error creating socket" << std::endl;
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  // Connect to localhost
  if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
  {
    std::cerr << "Invalid address" << std::endl;
    close(sockfd);
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    std::cerr << "Connection failed" << std::endl;
    close(sockfd);
    return -1;
  }

  return sockfd;
}

int main(int /* argc */, char * /* argv */[])
{
  std::cout << "LSM-Tree Database Client\n"
            << "------------------------\n"
            << "Connecting to server at 127.0.0.1:" << SERVER_PORT << "...\n";

  int sockfd = connect_to_server();
  if (sockfd < 0)
  {
    std::cerr << "Failed to connect to server. Make sure the server is running." << std::endl;
    return 1;
  }

  std::cout << "Connected to server!" << std::endl;
  print_help();

  char buffer[BUFFER_SIZE];
  std::string input;
  DSLParser parser; // Create a parser instance

  while (true)
  {
    std::cout << "\n> ";
    std::getline(std::cin, input);

    if (input.empty())
    {
      continue;
    }

    // Parse the command
    Command cmd = parser.parse(input); // Use the instance method

    // Check if the user wants to quit
    if (cmd.type == CommandType::EXIT)
    {
      std::cout << "Exiting client..." << std::endl;
      break;
    }

    // Handle help command locally
    if (cmd.type == CommandType::HELP)
    {
      print_help();
      continue;
    }

    // Send the command to the server
    if (send(sockfd, input.c_str(), input.length(), 0) < 0)
    {
      std::cerr << "Failed to send command to server" << std::endl;
      break;
    }

    // Receive and display the server's response
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_read <= 0)
    {
      std::cerr << "Connection closed by server" << std::endl;
      break;
    }

    std::cout << buffer;
  }

  close(sockfd);
  return 0;
}