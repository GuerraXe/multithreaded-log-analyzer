#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace la {

// Read-only view of a whole file. On Windows the bytes are memory-mapped
// (CreateFileMapping / MapViewOfFile); everywhere else, and whenever mapping
// fails or the file is empty, the bytes are read into an owned buffer. Either
// way `data()` is a contiguous view valid for the object's lifetime.
//
// Move-only. Construct with the static open() factory, which never throws.
class MappedFile {
public:
    static MappedFile open(const std::string& path);

    MappedFile() = default;
    ~MappedFile();
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const std::string& error() const { return error_; }

    std::string_view data() const { return {data_, size_}; }
    std::size_t size() const { return size_; }

    // True when the bytes came from a real memory mapping (diagnostic only).
    bool memory_mapped() const { return mapped_; }

private:
    void release() noexcept;
    void adopt(MappedFile&& other) noexcept;

    bool ok_ = false;
    bool mapped_ = false;
    std::string error_;
    const char* data_ = nullptr;
    std::size_t size_ = 0;

    // Win32 handles, kept as void* so <windows.h> stays out of this header.
    void* file_handle_ = nullptr;
    void* mapping_handle_ = nullptr;
    void* view_ = nullptr;

    std::string buffer_; // holds the bytes when !mapped_
};

} // namespace la
