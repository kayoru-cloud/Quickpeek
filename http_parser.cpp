#include "http_parser.hpp"
#include <algorithm>

namespace rapidserve {

HttpParser::HttpParser(ArenaAllocator<65536>& arena)
    : arena_(arena) {}

void HttpParser::reset() {
    complete_ = false;
    state_ = METHOD;
    method_buf_.clear();
    path_buf_.clear();
    current_header_name_.clear();
    current_header_value_.clear();
    request_ = HttpRequest{};
    body_bytes_read_ = 0;
    expected_body_size_ = 0;
}

void HttpParser::feed(const char* data, size_t len) {
    for (size_t i = 0; i < len && !complete_; ++i) {
        process_char(data[i]);
    }
}

void HttpParser::process_char(char c) {
    switch (state_) {
        case METHOD:
            if (c == ' ') {
                request_.method = method_buf_;
                state_ = PATH;
            } else {
                method_buf_ += c;
            }
            break;
        case PATH:
            if (c == ' ') {
                request_.path = path_buf_;
                state_ = VERSION;
            } else {
                path_buf_ += c;
            }
            break;
        case VERSION:
            if (c == '\r') break;
            else if (c == '\n') {
                state_ = HEADER_NAME;
            }
            // ignore version string
            break;
        case HEADER_NAME:
            if (c == ':') {
                state_ = HEADER_VALUE;
            } else if (c == '\r') {
                // empty line, end of headers
                state_ = BODY;  // could be DONE if no body
                finalize_request();
            } else if (c != '\n') {
                current_header_name_ += c;
            }
            break;
        case HEADER_VALUE:
            if (c == '\r') {
                // trim leading space
                if (!current_header_value_.empty() && current_header_value_[0] == ' ') {
                    current_header_value_.erase(0, 1);
                }
                request_.headers[current_header_name_] = current_header_value_;
                current_header_name_.clear();
                current_header_value_.clear();
                state_ = HEADER_NAME;
            } else if (c == '\n') {
                // ignore
            } else {
                current_header_value_ += c;
            }
            break;
        case BODY:
            complete_ = true;
            break;
        default:
            break;
    }
}

void HttpParser::finalize_request() {
    auto it = request_.headers.find("Content-Length");
    if (it != request_.headers.end()) {
        expected_body_size_ = std::stoul(it->second);
        if (expected_body_size_ == 0) {
            complete_ = true;
        } else {
            state_ = BODY;  
        }
    } else {
        complete_ = true;
    }
}

HttpRequest HttpParser::get_parsed_request() {
    return request_;
}

} 
