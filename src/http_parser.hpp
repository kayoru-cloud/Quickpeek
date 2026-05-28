#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <cstring>
#include "arena_allocator.hpp"

namespace rapidserve {

struct HttpRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;               // for this demo we store body as string (arena‑backed later)
};

class HttpParser {
public:
    HttpParser(ArenaAllocator<65536>& arena);

    void feed(const char* data, size_t len);
    bool is_complete() const { return complete_; }
    HttpRequest get_parsed_request();
    void reset();

private:
    enum State {
        METHOD,
        PATH,
        VERSION,
        HEADER_NAME,
        HEADER_VALUE,
        BODY,
        DONE
    };

    ArenaAllocator<65536>& arena_;
    bool complete_ = false;
    HttpRequest request_;               // we'll fill this during parsing
    State state_ = METHOD;

    // Temporary buffers for line parsing
    std::string current_header_name_;
    std::string current_header_value_;
    std::string method_buf_;
    std::string path_buf_;
    size_t body_bytes_read_ = 0;
    size_t expected_body_size_ = 0;

    void process_char(char c);
    void finalize_request();
};

} // namespace rapidserve
