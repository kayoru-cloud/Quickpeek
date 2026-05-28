#pragma once
#include "http_parser.hpp"
#include <functional>
#include <string>
#include <unordered_map>

namespace rapidserve {

struct HttpResponse {
    int status_code = 200;
    std::string content_type = "text/plain";
    std::string body;

    std::string to_string() const;
};

class Router {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    void add_route(const std::string& path, Handler handler);
    HttpResponse dispatch(const HttpRequest& req) const;

private:
    std::unordered_map<std::string, Handler> routes_;
};

} 
