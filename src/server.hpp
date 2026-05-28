#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <unordered_map>
#include <netinet/in.h>
#include "connection.hpp"

namespace rapidserve {

class ThreadPool;
class Router;
class FileCache;

class Server {
public:
    Server(std::string ip, uint16_t port, size_t num_workers, const std::string& static_dir);
    ~Server();
    void run();
    void stop();

  
    Router& router() { return *router_; }

private:
    void event_loop();

    int listen_fd_;
    int epoll_fd_;
    std::atomic<bool> running_{false};
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<Router> router_;
    std::unique_ptr<FileCache> file_cache_;
    std::vector<std::thread> io_threads_;
    std::string ip_;
    uint16_t port_;


    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
};

} // namespace rapidserve
