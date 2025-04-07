#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>

// Include all implementations
#include "naive/lsm_tree.h"
// Uncomment as they are implemented
//#include "compaction/lsm_tree.h"
//#include "bloom/lsm_tree.h"
//#include "fence/lsm_tree.h"
//#include "concurrency/lsm_tree.h"

namespace fs = std::filesystem;

// Interface for LSM-Tree implementations
class LSMTreeInterface {
public:
    virtual ~LSMTreeInterface() = default;
    virtual void put(const std::string& key, const std::string& value) = 0;
    virtual std::optional<std::string> get(const std::string& key) = 0;
    virtual void range(const std::string& start_key, const std::string& end_key, 
                      const std::function<void(const std::string&, const std::string&)>& callback) = 0;
    virtual void remove(const std::string& key) = 0;
    virtual void flush() = 0;
    virtual void compact() = 0;
    virtual void close() = 0;
    virtual void clear() = 0;
    virtual std::string getStats() = 0;
    virtual size_t getSStableCount() = 0;
    virtual size_t getMemTableSize() = 0;
    virtual size_t getTotalSizeBytes() = 0;
};

// Factory for creating LSM-Tree instances
class LSMTreeFactory {
public:
    static std::unique_ptr<LSMTreeInterface> create(const std::string& implementation, 
                                                  const std::string& data_dir,
                                                  size_t memtable_size_bytes) {
        if (implementation == "naive") {
            return std::make_unique<NaiveLSMTree>(data_dir, memtable_size_bytes);
        } 
        // Add other implementations as they are developed
        else {
            throw std::runtime_error("Unknown implementation: " + implementation);
        }
    }

private:
    // Wrapper for the naive implementation that implements LSMTreeInterface
    class NaiveLSMTree : public LSMTreeInterface {
    public:
        NaiveLSMTree(const std::string& data_dir, size_t memtable_size_bytes)
            : tree(data_dir, memtable_size_bytes) {}

        void put(const std::string& key, const std::string& value) override {
            tree.put(key, value);
        }

        std::optional<std::string> get(const std::string& key) override {
            return tree.get(key);
        }

        void range(const std::string& start_key, const std::string& end_key, 
                  const std::function<void(const std::string&, const std::string&)>& callback) override {
            tree.range(start_key, end_key, callback);
        }

        void remove(const std::string& key) override {
            tree.remove(key);
        }

        void flush() override {
            tree.flush();
        }

        void compact() override {
            tree.compact();
        }

        void close() override {
            tree.close();
        }

        void clear() override {
            tree.clear();
        }

        std::string getStats() override {
            return tree.getStats();
        }

        size_t getSStableCount() override {
            return tree.getSStableCount();
        }

        size_t getMemTableSize() override {
            return tree.getMemTableSize();
        }

        size_t getTotalSizeBytes() override {
            return tree.getTotalSizeBytes();
        }

    private:
        lsm::naive::LSMTree tree;
    };

    // Add wrapper classes for other implementations as they are developed
};

void printHelp() {
    std::cout << "LSM-Tree Command Line Interface\n";
    std::cout << "--------------------------------\n";
    std::cout << "Available commands:\n";
    std::cout << "  p <key> <value>      - Put a key-value pair\n";
    std::cout << "  g <key>              - Get value for a key\n";
    std::cout << "  r <start> <end>      - Range query from start to end key\n";
    std::cout << "  d <key>              - Delete a key\n";
    std::cout << "  f                    - Flush MemTable to disk\n";
    std::cout << "  c                    - Trigger compaction\n";
    std::cout << "  s                    - Show statistics\n";
    std::cout << "  l <file>             - Load commands from file\n";
    std::cout << "  q                    - Quit\n";
    std::cout << "  h                    - Show this help\n";
}

int main(int argc, char* argv[]) {
    // Default values
    std::string implementation = "naive";
    std::string data_dir = "project/data/cli";
    size_t memtable_size_bytes = 1024 * 1024; // 1MB
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--implementation" || arg == "-i") {
            if (i + 1 < argc) {
                implementation = argv[++i];
            }
        } else if (arg == "--data-dir" || arg == "-d") {
            if (i + 1 < argc) {
                data_dir = argv[++i];
            }
        } else if (arg == "--memtable-size" || arg == "-m") {
            if (i + 1 < argc) {
                memtable_size_bytes = std::stoul(argv[++i]);
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --implementation, -i <impl>   Implementation to use (naive, compaction, bloom, fence, concurrency)\n";
            std::cout << "  --data-dir, -d <path>         Directory to store data files\n";
            std::cout << "  --memtable-size, -m <bytes>   Maximum size of MemTable in bytes\n";
            std::cout << "  --help, -h                    Show this help\n";
            return 0;
        }
    }
    
    // Create data directory if it doesn't exist
    fs::create_directories(data_dir);
    
    // Create LSM-Tree instance
    std::cout << "Using " << implementation << " implementation\n";
    std::cout << "Data directory: " << data_dir << "\n";
    std::cout << "MemTable size: " << memtable_size_bytes << " bytes\n";
    
    std::unique_ptr<LSMTreeInterface> lsm;
    try {
        lsm = LSMTreeFactory::create(implementation, data_dir, memtable_size_bytes);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "LSM-Tree initialized. Type 'h' for help.\n";
    
    // Main command loop
    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command == "p" || command == "put") {
            std::string key, value;
            if (iss >> key >> value) {
                lsm->put(key, value);
                std::cout << "Put: " << key << " -> " << value << "\n";
            } else {
                std::cout << "Usage: p <key> <value>\n";
            }
        } else if (command == "g" || command == "get") {
            std::string key;
            if (iss >> key) {
                auto value = lsm->get(key);
                if (value) {
                    std::cout << "Get: " << key << " -> " << *value << "\n";
                } else {
                    std::cout << "Key not found: " << key << "\n";
                }
            } else {
                std::cout << "Usage: g <key>\n";
            }
        } else if (command == "r" || command == "range") {
            std::string start_key, end_key;
            if (iss >> start_key >> end_key) {
                std::cout << "Range: " << start_key << " to " << end_key << "\n";
                size_t count = 0;
                lsm->range(start_key, end_key, [&count](const std::string& key, const std::string& value) {
                    std::cout << "  " << key << " -> " << value << "\n";
                    count++;
                });
                std::cout << count << " results found\n";
            } else {
                std::cout << "Usage: r <start_key> <end_key>\n";
            }
        } else if (command == "d" || command == "delete") {
            std::string key;
            if (iss >> key) {
                lsm->remove(key);
                std::cout << "Deleted: " << key << "\n";
            } else {
                std::cout << "Usage: d <key>\n";
            }
        } else if (command == "f" || command == "flush") {
            lsm->flush();
            std::cout << "MemTable flushed\n";
        } else if (command == "c" || command == "compact") {
            lsm->compact();
            std::cout << "Compaction triggered\n";
        } else if (command == "s" || command == "stats") {
            std::cout << "Statistics:\n";
            std::cout << "  MemTable size: " << lsm->getMemTableSize() << " bytes\n";
            std::cout << "  SSTable count: " << lsm->getSStableCount() << "\n";
            std::cout << "  Total size: " << lsm->getTotalSizeBytes() << " bytes\n";
            std::cout << "  Details: " << lsm->getStats() << "\n";
        } else if (command == "l" || command == "load") {
            std::string filename;
            if (iss >> filename) {
                std::ifstream file(filename);
                if (file.is_open()) {
                    std::cout << "Loading commands from " << filename << "\n";
                    std::string cmd_line;
                    while (std::getline(file, cmd_line)) {
                        std::cout << "> " << cmd_line << "\n";
                        std::istringstream cmd_iss(cmd_line);
                        std::string cmd;
                        cmd_iss >> cmd;
                        
                        if (cmd == "p" || cmd == "put") {
                            std::string key, value;
                            if (cmd_iss >> key >> value) {
                                lsm->put(key, value);
                                std::cout << "Put: " << key << " -> " << value << "\n";
                            }
                        } else if (cmd == "g" || cmd == "get") {
                            std::string key;
                            if (cmd_iss >> key) {
                                auto value = lsm->get(key);
                                if (value) {
                                    std::cout << "Get: " << key << " -> " << *value << "\n";
                                } else {
                                    std::cout << "Key not found: " << key << "\n";
                                }
                            }
                        } else if (cmd == "d" || cmd == "delete") {
                            std::string key;
                            if (cmd_iss >> key) {
                                lsm->remove(key);
                                std::cout << "Deleted: " << key << "\n";
                            }
                        } else if (cmd == "f" || cmd == "flush") {
                            lsm->flush();
                            std::cout << "MemTable flushed\n";
                        }
                        // Other commands omitted for simplicity in batch mode
                    }
                    std::cout << "Finished loading commands\n";
                } else {
                    std::cout << "Error: Could not open file " << filename << "\n";
                }
            } else {
                std::cout << "Usage: l <filename>\n";
            }
        } else if (command == "h" || command == "help") {
            printHelp();
        } else if (command == "q" || command == "quit" || command == "exit") {
            break;
        } else if (!command.empty()) {
            std::cout << "Unknown command: " << command << "\n";
            std::cout << "Type 'h' for help\n";
        }
    }
    
    // Clean up
    lsm->close();
    std::cout << "LSM-Tree closed\n";
    
    return 0;
} 