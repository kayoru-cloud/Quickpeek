#include "server.hpp"
#include "thread_pool.hpp"
#include "router.hpp"
#include "file_cache.hpp"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace rapidserve {

Server::Server(std::string ip, uint16_t port, size_t num_workers, const std::string& static_dir)
    : ip_(std::move(ip)), port_(port)
{
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");
    if (listen(listen_fd_, SOMAXCONN) < 0)
        throw std::runtime_error("listen() failed");

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) throw std::runtime_error("epoll_create1() failed");

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);

    thread_pool_ = std::make_unique<ThreadPool>(num_workers);
    router_ = std::make_unique<Router>();
    file_cache_ = std::make_unique<FileCache>();

    // Load static files
    if (!static_dir.empty()) {
        size_t loaded = file_cache_->load_directory(static_dir);
        std::cout << "Loaded " << loaded << " static files from " << static_dir << "\n";
    }

    router_->add_route("/", [](const HttpRequest&) -> HttpResponse {
        return {200, "text/html", 
            "<!DOCTYPE html><html><head><title>RapidServe</title></head>"
            "<body><h1>RapidServe is running!</h1>"
            "<p>This page was served by a high‑performance C++ backend.</p></body></html>"};
    });
    router_->add_route("/api/hello", [](const HttpRequest&) -> HttpResponse {
        return {200, "application/json", "{\"message\": \"Hello, world!\"}"};
    });

    // Spawn one I/O thread (could be multiple with SO_REUSEPORT)
    io_threads_.emplace_back(&Server::event_loop, this);
}

Server::~Server() {
    stop();
}

void Server::run() {
    running_ = true;
    for (auto& t : io_threads_) if (t.joinable()) t.join();
}

void Server::stop() {
    if (!running_) return;
    running_ = false;
    shutdown(listen_fd_, SHUT_RDWR);
    close(listen_fd_);
    close(epoll_fd_);
    thread_pool_->stop();
    // Join IO threads
    for (auto& t : io_threads_) if (t.joinable()) t.join();
}

void Server::event_loop() {
    constexpr int MAX_EVENTS = 128;
    epoll_event events[MAX_EVENTS];

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == listen_fd_) {
                // Accept all pending connections (edge‑triggered)
                while (true) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept4(listen_fd_, (sockaddr*)&client_addr,
                                           &client_len, SOCK_NONBLOCK);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        continue;
                    }

                    // Set TCP optimisations
                    int opt = 1;
                    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
                    setsockopt(client_fd, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt));

                    auto conn = std::make_shared<Connection>(client_fd, *thread_pool_, *router_, *file_cache_);
                    connections_[client_fd] = conn;

                    epoll_event ev{};
                    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;   
                    ev.data.ptr = conn.get();
                    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
                }
            } else {
                // Find connection by raw pointer (still valid because we only delete after erasing from map)
                auto it = connections_.find(fd);
                if (it == connections_.end()) continue; // already closed
                auto* conn_raw = static_cast<Connection*>(events[i].data.ptr);
                // In case the connection was closed while we were waiting (race), check state
                if (conn_raw != it->second.get()) continue;

                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    // Client disconnected or error
                    conn_raw->close();
                    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                    connections_.erase(fd);
                } else {
                    if (events[i].events & EPOLLIN)  conn_raw->handle_read();
                    if (events[i].events & EPOLLOUT) conn_raw->handle_write();
                    // If the connection closes,remove it
                    if (conn_raw->fd() == -1) {
                        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                        connections_.erase(fd);
                    }
                }
            }
        }
    }
    // Cleanup remaining connections
    for (auto& [fd, conn] : connections_) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        conn->close();
    }
    connections_.clear();
}

} 
