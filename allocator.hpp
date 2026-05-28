#pragma once
#include <cstddef>
#include <cassert>
#include <new>

namespace rapidserve {

template<size_t Size>
class ArenaAllocator {
public:
    ArenaAllocator() noexcept : offset_(0) {}

    void* allocate(size_t bytes) noexcept {
        if (offset_ + bytes > Size) return nullptr;
        void* ptr = buffer_ + offset_;
        offset_ += bytes;
        return ptr;
    }

    char* strdup(const char* src, size_t len) {
        char* dst = static_cast<char*>(allocate(len + 1));
        if (dst) {
            memcpy(dst, src, len);
            dst[len] = '\0';
        }
        return dst;
    }

    void reset() noexcept { offset_ = 0; }
    size_t used() const noexcept { return offset_; }

private:
    alignas(64) char buffer_[Size];
    size_t offset_;
};

} 
