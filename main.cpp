#include "server.hpp"
#include "router.hpp"
#include <iostream>
#include <csignal>
#include <string>

rapidserve::Server* g_server = nullptr;

void signal_handler(int) {
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    size_t threads = std::thread::hardware_concurrency();
    std::string static_dir = "static";
  
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i+1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--threads" && i+1 < argc) threads = std::stoi(argv[++i]);
        else if (arg == "--static" && i+1 < argc) static_dir = argv[++i];
    }

    try {
        rapidserve::Server server("0.0.0.0", port, threads, static_dir);
        g_server = &server;

        // Register signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        std::cout << "RapidServe listening on port " << port 
                  << " with " << threads << " worker threads\n";
        server.run();
        std::cout << "Server stopped.\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
