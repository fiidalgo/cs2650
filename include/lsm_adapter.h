#ifndef LSM_ADAPTER_H
#define LSM_ADAPTER_H

#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include "lsm_tree.h"

namespace lsm
{

    // Adapter to connect the LSM-tree with the server
    class LSMAdapter
    {
    public:
        // Get the singleton instance
        static LSMAdapter &get_instance();

        // Delete the copy/move constructors and assignments
        LSMAdapter(const LSMAdapter &) = delete;
        LSMAdapter &operator=(const LSMAdapter &) = delete;
        LSMAdapter(LSMAdapter &&) = delete;
        LSMAdapter &operator=(LSMAdapter &&) = delete;

        // Process commands from the server
        std::string process_command(const std::string &command);

        // Safely shutdown the LSM adapter and tree
        void shutdown();

    private:
        // Private constructor for singleton
        LSMAdapter();

        // The LSM-tree instance
        std::unique_ptr<LSMTree> tree;

        // Command handlers
        std::string handle_put(const std::vector<std::string> &tokens);
        std::string handle_get(const std::vector<std::string> &tokens);
        std::string handle_range(const std::vector<std::string> &tokens);
        std::string handle_delete(const std::vector<std::string> &tokens);
        std::string handle_load(const std::string &command);
        std::string handle_stats();

        // Helper to parse tokens
        std::vector<std::string> tokenize(const std::string &command) const;
    };

} // namespace lsm

#endif // LSM_ADAPTER_H