#pragma once
#include <string>
#include <unordered_map>
#include <string_view>

namespace rapidserve {

class FileCache {
public:
    size_t load_directory(const std::string& dir_path);
    const std::string* get(const std::string& path) const;

    // Guess content type from file extension.
    static std::string content_type(std::string_view path);

private:
    std::unordered_map<std::string, std::string> files_; 
};

} 
