# LSM-Tree

This project implements a client-server architecture for an LSM-Tree (Log-Structured Merge-Tree).

## Project Structure

- `include/`: Header files

  - `bloom_filter.h`: Bloom filter implementation for efficient lookups
  - `client.h`: Client class definition
  - `constants.h`: System-wide constants and DSL definitions
  - `fence_pointers.h`: Fence pointers for run indexing
  - `lsm_adapter.h`: LSM-Tree adapter interface
  - `lsm_tree.h`: Core LSM-Tree implementation
  - `run.h`: Run management and operations
  - `server.h`: Server class definition
  - `skip_list.h`: Skip list implementation for memory buffer
  - `thread_pool.h`: Thread pool implementation

- `src/`: Source files
  - `bloom_filter.cpp`: Bloom filter implementation
  - `client.cpp`: Client implementation
  - `data_generator.cpp`: Test data generation utilities
  - `data_generator_256mb.cpp`: Large dataset generator
  - `fence_pointers.cpp`: Fence pointer implementation
  - `lsm_adapter.cpp`: LSM-Tree adapter implementation
  - `lsm_tree.cpp`: Core LSM-Tree functionality
  - `main_client.cpp`: Client entry point
  - `main_server.cpp`: Server entry point
  - `run.cpp`: Run operations implementation
  - `server.cpp`: Server implementation with command processing
  - `skip_list.cpp`: Skip list implementation
  - `thread_pool.cpp`: Thread pool implementation
  - `almost_full_buffer_generator.cpp`: Buffer testing utility
  - `generate_test_data.cpp`: Test data generation tools

## Building the Project

To build all components of the project, run:

```bash
make
```

This will create the following executables in the `bin/` directory:

- `server`: The LSM-Tree server
- `client`: A client to interact with the server
- `generate_test_data`: Utility for generating test data with different sizes and distributions
- `data_generator`: Utility for generating 10GB test data
- `data_generator_256mb`: Utility for generating 256MB test data
- `almost_full_buffer_generator`: Utility for testing buffer management

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

### Generating Test Data

The project includes several utilities for generating test data:

1. Generate test data with specific size and distribution:

```bash
make generate-data
```

2. Generate a comprehensive test dataset:

```bash
make generate-test-data-all
```

This creates multiple files with different sizes (100MB, 256MB, 512MB, 1024MB) and distributions (uniform, skewed).

3. Generate large datasets:

```bash
make generate-10gb      # Generates 10GB test data
make generate-256mb     # Generates 256MB test data
```

4. Generate data for buffer testing:

```bash
make generate-almost-full
```

### Performance Testing

Run performance tests across all dimensions:

```bash
make performance-test
```

Run performance test for a specific dimension:

```bash
make performance-test-dim DIMENSION=data_size
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
