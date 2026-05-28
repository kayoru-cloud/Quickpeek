#include "router.hpp"

namespace rapidserve {

std::string HttpResponse::to_string() const {
    std::string response;
    response.reserve(128 + body.size());
    response += "HTTP/1.1 " + std::to_string(status_code) + " OK\r\n";
    response += "Content-Type: " + content_type + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: keep-alive\r\n\r\n";
    response += body;
    return response;
}

void Router::add_route(const std::string& path, Handler handler) {
    routes_[path] = std::move(handler);
}

HttpResponse Router::dispatch(const HttpRequest& req) const {
    
    auto it = routes_.find(req.path);
    if (it != routes_.end()) {
        return it->second(req);
    }
    return {404, "text/plain", "Not Found"};
}

} 
