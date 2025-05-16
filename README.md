# LSM-Tree Implementation

This project implements a client-server architecture for an LSM-Tree (Log-Structured Merge-Tree) with a well-defined Domain-Specific Language (DSL) for interacting with the tree.

## Current State

- Client-server architecture fully implemented
- Thread pool for handling concurrent client requests
- Command parsing and validation
- Input validation with error handling
- Domain-Specific Language (DSL) interface

The actual LSM-Tree implementation is planned for future development.

## Project Structure

- `include/`: Header files
  - `constants.h`: System-wide constants and DSL definitions
  - `thread_pool.h`: Thread pool implementation
  - `server.h`: Server class definition
  - `client.h`: Client class definition
- `src/`: Source files
  - `thread_pool.cpp`: Thread pool implementation
  - `server.cpp`: Server implementation with command processing
  - `client.cpp`: Client implementation
  - `main_server.cpp`: Server entry point
  - `main_client.cpp`: Client entry point

## Building the Project

To build the project, run:

```bash
make
```

This will create two executables in the `bin/` directory:

- `server`: The LSM-Tree server
- `client`: A client to interact with the server

## Running the Project

### Starting the Server

To start the server with the default port (9090):

```bash
make run-server
```

Or specify a custom port:

```bash
./bin/server 9091
```

### Running a Client

To run a client and connect to a local server:

```bash
make run-client
```

Or specify a custom host and port:

```bash
./bin/client 192.168.1.100 9091
```

## Domain-Specific Language (DSL)

The LSM-Tree supports the following commands:

### Put Command

Insert or update a key-value pair in the tree.

```
p [key] [value]
```

Example: `p 10 7` – Stores key 10 with value 7

### Get Command

Retrieve a value for a given key.

```
g [key]
```

Example: `g 10` – Gets the value associated with key 10

### Range Query

Retrieve all key-value pairs within a range (inclusive start, exclusive end).

```
r [start] [end]
```

Example: `r 10 20` – Gets all key-value pairs with keys from 10 to 19

### Delete Command

Remove a key-value pair from the tree.

```
d [key]
```

Example: `d 10` – Deletes the entry with key 10

### Load Command

Load key-value pairs from a binary file.

```
l "[filepath]"
```

Example: `l "/path/to/file.bin"` – Loads pairs from the specified file

### Statistics Command

Print statistics about the current state of the tree.

```
s
```

### Help Command

Display help information about available commands.

```
h
```

### Quit Command

Disconnect from the server.

```
q
```

## Architecture

- **Server**: Listens on a specified port for client connections

  - Supports up to 64 concurrent clients
  - Uses a thread pool to process client commands in parallel
  - Command processing with strict validation

- **Client**: Connects to the server and provides a command interface

  - DSL for interacting with the LSM-Tree
  - Error handling and command validation
  - Command-response pattern

- **Thread Pool**: Enables parallel processing of client commands
  - Worker threads take tasks from a queue
  - Asynchronous task completion with futures

## Future Development

The next phase of the project will implement the actual LSM-Tree data structure, including:

- In-memory buffer (memtable)
- On-disk SSTables
- Bloom filters for efficient lookups
- Merging and compaction strategies
- Persistence and recovery

## License

[Add your license information here]
