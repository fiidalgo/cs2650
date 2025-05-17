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

        // Access the tree (needed for buffer size optimization)
        LSMTree *get_tree() const { return tree.get(); }

        // Safely shutdown the LSM adapter and tree
        void shutdown();

        // I/O tracking methods
        void increment_read_io()
        {
            if (tree)
                tree->increment_read_io();
        }

        void increment_write_io()
        {
            if (tree)
                tree->increment_write_io();
        }

        size_t get_read_io_count() const
        {
            return tree ? tree->get_read_io_count() : 0;
        }

        size_t get_write_io_count() const
        {
            return tree ? tree->get_write_io_count() : 0;
        }

        void reset_io_stats()
        {
            if (tree)
                tree->reset_io_stats();
        }

        // Operation timing metrics
        double get_avg_read_time_ms() const
        {
            return tree ? tree->get_avg_read_time_ms() : 0.0;
        }

        double get_avg_write_time_ms() const
        {
            return tree ? tree->get_avg_write_time_ms() : 0.0;
        }

        size_t get_read_count() const
        {
            return tree ? tree->get_read_count() : 0;
        }

        size_t get_write_count() const
        {
            return tree ? tree->get_write_count() : 0;
        }

        void reset_timing_stats()
        {
            if (tree)
                tree->reset_timing_stats();
        }

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
        std::string handle_reset_stats();

        // Helper to parse tokens
        std::vector<std::string> tokenize(const std::string &command) const;
    };

} // namespace lsm

#endif // LSM_ADAPTER_H