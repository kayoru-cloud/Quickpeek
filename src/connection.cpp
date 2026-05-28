#include "connection.hpp"
#include "thread_pool.hpp"
#include "router.hpp"
#include "file_cache.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

namespace rapidserve {

Connection::Connection(int fd, ThreadPool& pool, Router& router, FileCache& cache)
    : fd_(fd), pool_(pool), router_(router), file_cache_(cache), parser_(arena_) {}

Connection::~Connection() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

void Connection::close() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
    state_ = CLOSED;
}

void Connection::handle_read() {
    while (state_ == READING) {
        ssize_t n = recv(fd_, read_buf_.data(), read_buf_.size(), 0);
        if (n > 0) {
            parser_.feed(read_buf_.data(), n);
            if (parser_.is_complete()) {
                auto req = parser_.get_parsed_request();
                state_ = PROCESSING;

                // Capture shared_ptr to keep connection alive while processing
                auto self = shared_from_this();
                pool_.enqueue([self, req = std::move(req)]() mutable {
                    // First try to serve from static file cache if path starts with "/static/"
                    if (req.path.rfind("/static/", 0) == 0) {
                        std::string file_path = req.path.substr(8); // skip "/static/"
                        if (file_path.empty()) file_path = "index.html";
                        const std::string* content = self->file_cache_.get(file_path);
                        if (content) {
                            self->response_ = HttpResponse{200,
                                FileCache::content_type(file_path),
                                *content}.to_string();
                        } else {
                            self->response_ = HttpResponse{404, "text/plain", "File not found"}.to_string();
                        }
                    } else {
                        // Dispatch to dynamic routes
                        auto res = self->router_.dispatch(req);
                        self->response_ = res.to_string();
                    }
                    self->state_ = WRITING;
                    // I/O thread will notice state change and try to write
                });
                return; // stop reading until response is sent
            }
        } else if (n == 0) {
            // Client closed connection
            close();
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close();
            return;
        }
    }
}

void Connection::handle_write() {
    if (state_ != WRITING) return;
    while (response_sent_ < response_.size()) {
        ssize_t n = send(fd_, response_.data() + response_sent_,
                         response_.size() - response_sent_, MSG_NOSIGNAL);
        if (n > 0) {
            response_sent_ += n;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else {
            close();
            return;
        }
    }
    if (response_sent_ == response_.size()) {
        // Reset for next request (keep‑alive)
        parser_.reset();
        arena_.reset();
        response_.clear();
        response_sent_ = 0;
        state_ = READING;
    }
}

} // namespace rapidserve
