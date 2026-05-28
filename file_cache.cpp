#include "file_cache.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace rapidserve {

size_t FileCache::load_directory(const std::string& dir_path) {
    namespace fs = std::filesystem;
    size_t count = 0;
    try {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
                if (!file) continue;
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);
                std::string content(size, '\0');
                if (file.read(&content[0], size)) {
                
                    std::string rel = entry.path().filename().string();
                    files_[rel] = std::move(content);
                    ++count;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "FileCache error: " << e.what() << std::endl;
    }
    return count;
}

const std::string* FileCache::get(const std::string& path) const {
    auto it = files_.find(path);
    if (it != files_.end()) return &it->second;
    return nullptr;
}

std::string FileCache::content_type(std::string_view path) {
    if (path.ends_with(".html")) return "text/html; charset=utf-8";
    if (path.ends_with(".css"))  return "text/css; charset=utf-8";
    if (path.ends_with(".js"))   return "application/javascript; charset=utf-8";
    if (path.ends_with(".json")) return "application/json; charset=utf-8";
    if (path.ends_with(".png"))  return "image/png";
    if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
    if (path.ends_with(".svg"))  return "image/svg+xml";
    return "application/octet-stream";
}

} 
