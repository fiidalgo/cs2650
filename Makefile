CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
INCLUDES = -I./include

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Create directories if they don't exist
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# Server objects
SERVER_OBJS = $(OBJ_DIR)/server.o $(OBJ_DIR)/thread_pool.o $(OBJ_DIR)/main_server.o

# Client objects
CLIENT_OBJS = $(OBJ_DIR)/client.o $(OBJ_DIR)/main_client.o

# Targets
all: $(BIN_DIR)/server $(BIN_DIR)/client

# Server executable
$(BIN_DIR)/server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Client executable
$(BIN_DIR)/client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generic rule for object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# Clean
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*

# Run server
run-server: $(BIN_DIR)/server
	$(BIN_DIR)/server

# Run client
run-client: $(BIN_DIR)/client
	$(BIN_DIR)/client

.PHONY: all clean run-server run-client 