CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
INCLUDES = -I./include

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Create directories if they don't exist
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# LSM-tree objects
LSM_OBJS = $(OBJ_DIR)/lsm_adapter.o $(OBJ_DIR)/lsm_tree.o $(OBJ_DIR)/skip_list.o \
           $(OBJ_DIR)/bloom_filter.o $(OBJ_DIR)/fence_pointers.o $(OBJ_DIR)/run.o

# Server objects
SERVER_OBJS = $(OBJ_DIR)/server.o $(OBJ_DIR)/thread_pool.o $(OBJ_DIR)/main_server.o $(LSM_OBJS)

# Client objects
CLIENT_OBJS = $(OBJ_DIR)/client.o $(OBJ_DIR)/main_client.o

# Test data generator
TEST_DATA_OBJS = $(OBJ_DIR)/generate_test_data.o

# 10GB Data generator
DATA_GEN_OBJS = $(OBJ_DIR)/data_generator.o

# 256MB Data generator
DATA_GEN_256MB_OBJS = $(OBJ_DIR)/data_generator_256mb.o

# Almost full buffer generator
ALMOST_FULL_OBJS = $(OBJ_DIR)/almost_full_buffer_generator.o

# Targets
all: $(BIN_DIR)/server $(BIN_DIR)/client $(BIN_DIR)/generate_test_data $(BIN_DIR)/data_generator $(BIN_DIR)/data_generator_256mb $(BIN_DIR)/almost_full_buffer_generator

# Server executable
$(BIN_DIR)/server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Client executable
$(BIN_DIR)/client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Test data generator executable
$(BIN_DIR)/generate_test_data: $(TEST_DATA_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 10GB Data generator executable
$(BIN_DIR)/data_generator: $(DATA_GEN_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 256MB Data generator executable
$(BIN_DIR)/data_generator_256mb: $(DATA_GEN_256MB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Almost full buffer generator executable
$(BIN_DIR)/almost_full_buffer_generator: $(ALMOST_FULL_OBJS)
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

# Generate test data
generate-data: $(BIN_DIR)/generate_test_data
	$(BIN_DIR)/generate_test_data data/test_data.bin 10000

# Generate 10GB test data
generate-10gb: $(BIN_DIR)/data_generator
	$(BIN_DIR)/data_generator data/test_data_10gb.bin

# Generate 256MB test data
generate-256mb: $(BIN_DIR)/data_generator_256mb
	$(BIN_DIR)/data_generator_256mb data/test_data_256mb.bin

# Generate almost full buffer file
generate-almost-full: $(BIN_DIR)/almost_full_buffer_generator
	$(BIN_DIR)/almost_full_buffer_generator data/almost_full_buffer.bin

.PHONY: all clean run-server run-client generate-data generate-10gb generate-256mb generate-almost-full 