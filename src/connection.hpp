#pragma once
#include <memory>
#include <array>
#include <string>
#include "arena_allocator.hpp"
#include "http_parser.hpp"

namespace rapidserve {

class ThreadPool;
class Router;
class FileCache;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(int fd, ThreadPool& pool, Router& router, FileCache& cache);
    ~Connection();

    void handle_read();
    void handle_write();
    void close();

    int fd() const { return fd_; }

private:
    int fd_;
    ThreadPool& pool_;
    Router& router_;
    FileCache& file_cache_;
    ArenaAllocator<65536> arena_;      
    HttpParser parser_;
    enum State { READING, PROCESSING, WRITING, CLOSED } state_ = READING;

    std::array<char, 4096> read_buf_;
    std::string response_;
    size_t response_sent_ = 0;
};

} // namespace rapidserve
