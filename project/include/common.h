#ifndef LSM_COMMON_H
#define LSM_COMMON_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <random>
#include <cassert>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

namespace lsm
{

    // Type definitions
    using Key = int64_t;
    using Value = int64_t;
    using Timestamp = uint64_t;

    // Constants
    constexpr size_t DEFAULT_MEMTABLE_SIZE = 4 * 1024 * 1024; // 4MB
    constexpr size_t DEFAULT_MEMTABLE_ENTRIES = 1000000;      // 1M entries
    constexpr size_t DEFAULT_BLOCK_SIZE = 4 * 1024;           // 4KB
    constexpr size_t DEFAULT_LEVEL_SIZE_RATIO = 10;           // Size ratio between levels
    constexpr size_t MAX_LEVEL = 7;                           // Maximum number of levels

    // I/O tracking for experiments
    class IOTracker
    {
    public:
        static IOTracker &getInstance()
        {
            static IOTracker instance;
            return instance;
        }

        void recordRead(size_t bytes)
        {
            read_count_++;
            read_bytes_ += bytes;
        }

        void recordWrite(size_t bytes)
        {
            write_count_++;
            write_bytes_ += bytes;
        }

        void reset()
        {
            read_count_ = 0;
            write_count_ = 0;
            read_bytes_ = 0;
            write_bytes_ = 0;
        }

        size_t getReadCount() const { return read_count_; }
        size_t getWriteCount() const { return write_count_; }
        size_t getReadBytes() const { return read_bytes_; }
        size_t getWriteBytes() const { return write_bytes_; }

    private:
        IOTracker() : read_count_(0), write_count_(0), read_bytes_(0), write_bytes_(0) {}

        std::atomic<size_t> read_count_;
        std::atomic<size_t> write_count_;
        std::atomic<size_t> read_bytes_;
        std::atomic<size_t> write_bytes_;
    };

    // Key-Value pair structure
    struct KeyValue
    {
        Key key;
        Value value;
        bool is_deleted;

        KeyValue() : key(0), value(0), is_deleted(false) {}
        KeyValue(Key k, Value v, bool deleted = false)
            : key(k), value(v), is_deleted(deleted) {}

        // Comparison operators
        bool operator<(const KeyValue &other) const
        {
            return key < other.key;
        }

        bool operator==(const KeyValue &other) const
        {
            return key == other.key;
        }
    };

    // Result status
    enum class Status
    {
        OK,
        NOT_FOUND,
        IO_ERROR,
        INVALID_ARGUMENT,
        NOT_SUPPORTED
    };

    // Utility functions
    inline std::string statusToString(Status status)
    {
        switch (status)
        {
        case Status::OK:
            return "OK";
        case Status::NOT_FOUND:
            return "Not found";
        case Status::IO_ERROR:
            return "I/O error";
        case Status::INVALID_ARGUMENT:
            return "Invalid argument";
        case Status::NOT_SUPPORTED:
            return "Not supported";
        default:
            return "Unknown status";
        }
    }

    // File utilities
    inline bool fileExists(const std::string &path)
    {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    inline size_t fileSize(const std::string &path)
    {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) != 0)
        {
            return 0;
        }
        return buffer.st_size;
    }

    inline bool createDirectory(const std::string &path)
    {
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
    }

    // Directory entry class
    class DirectoryEntry
    {
    public:
        DirectoryEntry(const std::string &path) : path_(path) {}

        bool is_regular_file() const
        {
            struct stat buffer;
            if (stat(path_.c_str(), &buffer) != 0)
            {
                return false;
            }
            return S_ISREG(buffer.st_mode);
        }

        std::string path() const
        {
            return path_;
        }

    private:
        std::string path_;
    };

    // Directory iterator class
    class DirectoryIterator
    {
    public:
        class Iterator
        {
        public:
            Iterator(DIR *dir, const std::string &base_path)
                : dir_(dir), base_path_(base_path), entry_(nullptr)
            {
                if (dir_)
                {
                    advance();
                }
            }

            Iterator() : dir_(nullptr), entry_(nullptr) {}

            ~Iterator()
            {
                if (dir_)
                {
                    closedir(dir_);
                }
            }

            bool operator!=(const Iterator &other) const
            {
                return dir_ != other.dir_ || entry_ != other.entry_;
            }

            Iterator &operator++()
            {
                advance();
                return *this;
            }

            DirectoryEntry operator*() const
            {
                return DirectoryEntry(base_path_ + "/" + entry_->d_name);
            }

        private:
            void advance()
            {
                if (dir_)
                {
                    entry_ = readdir(dir_);
                    // Skip . and ..
                    while (entry_ && (strcmp(entry_->d_name, ".") == 0 || strcmp(entry_->d_name, "..") == 0))
                    {
                        entry_ = readdir(dir_);
                    }

                    if (!entry_)
                    {
                        closedir(dir_);
                        dir_ = nullptr;
                    }
                }
            }

            DIR *dir_;
            std::string base_path_;
            struct dirent *entry_;
        };

        DirectoryIterator(const std::string &path) : path_(path) {}

        Iterator begin() const
        {
            DIR *dir = opendir(path_.c_str());
            return Iterator(dir, path_);
        }

        Iterator end() const
        {
            return Iterator();
        }

    private:
        std::string path_;
    };

    // Directory iteration function
    inline DirectoryIterator directoryIterator(const std::string &path)
    {
        return DirectoryIterator(path);
    }

    // Tracked file I/O wrapper
    class TrackedFile
    {
    public:
        TrackedFile(const std::string &path, bool read_only = true)
            : path_(path), read_only_(read_only)
        {
            if (read_only)
            {
                file_ = fopen(path.c_str(), "rb");
            }
            else
            {
                file_ = fopen(path.c_str(), "wb");
            }
        }

        ~TrackedFile()
        {
            if (file_)
            {
                fclose(file_);
            }
        }

        bool isOpen() const { return file_ != nullptr; }

        size_t read(void *buffer, size_t size)
        {
            if (!file_ || !buffer)
                return 0;
            size_t bytes_read = fread(buffer, 1, size, file_);
            IOTracker::getInstance().recordRead(bytes_read);
            return bytes_read;
        }

        size_t write(const void *buffer, size_t size)
        {
            if (!file_ || !buffer || read_only_)
                return 0;
            size_t bytes_written = fwrite(buffer, 1, size, file_);
            IOTracker::getInstance().recordWrite(bytes_written);
            return bytes_written;
        }

        bool seek(long offset, int whence = SEEK_SET)
        {
            if (!file_)
                return false;
            return fseek(file_, offset, whence) == 0;
        }

        long tell()
        {
            if (!file_)
                return -1;
            return ftell(file_);
        }

    private:
        std::string path_;
        FILE *file_;
        bool read_only_;
    };

} // namespace lsm

#endif // LSM_COMMON_H