#include "io/mapped_file.hpp"

#include <fstream>
#include <ios>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace la {
namespace {

// Read the whole file into `out`. Returns an error string, empty on success.
// Sizes the destination up front and reads once, so the bytes are not copied
// through an intermediate stream buffer.
std::string read_all(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return "cannot open '" + path + "'";

    const std::streamoff end = in.tellg();
    if (end < 0) return "cannot stat '" + path + "'";
    in.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(end));
    if (end > 0) in.read(out.data(), end);
    if (in.bad()) return "read error on '" + path + "'";
    out.resize(static_cast<std::size_t>(in.gcount()));
    return {};
}

} // namespace

MappedFile MappedFile::open(const std::string& path) {
    MappedFile mf;

#ifdef _WIN32
    HANDLE file = ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        mf.error_ = "cannot open '" + path + "'";
        return mf;
    }

    LARGE_INTEGER li;
    if (!::GetFileSizeEx(file, &li)) {
        ::CloseHandle(file);
        mf.error_ = "cannot stat '" + path + "'";
        return mf;
    }
    const unsigned long long file_size = static_cast<unsigned long long>(li.QuadPart);

    if (file_size == 0) {
        // CreateFileMapping cannot map a zero-length file; an empty view is fine.
        ::CloseHandle(file);
        mf.ok_ = true;
        mf.mapped_ = false;
        mf.data_ = mf.buffer_.data();
        mf.size_ = 0;
        return mf;
    }

    HANDLE mapping = ::CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping == nullptr) {
        ::CloseHandle(file);
        // Fall back to a buffered read rather than failing outright.
        std::string buf;
        const std::string err = read_all(path, buf);
        if (!err.empty()) {
            mf.error_ = err;
            return mf;
        }
        mf.buffer_ = std::move(buf);
        mf.ok_ = true;
        mf.data_ = mf.buffer_.data();
        mf.size_ = mf.buffer_.size();
        return mf;
    }

    void* view = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) {
        ::CloseHandle(mapping);
        ::CloseHandle(file);
        mf.error_ = "cannot map '" + path + "'";
        return mf;
    }

    mf.ok_ = true;
    mf.mapped_ = true;
    mf.file_handle_ = file;
    mf.mapping_handle_ = mapping;
    mf.view_ = view;
    mf.data_ = static_cast<const char*>(view);
    mf.size_ = static_cast<std::size_t>(file_size);
    return mf;
#else
    std::string buf;
    const std::string err = read_all(path, buf);
    if (!err.empty()) {
        mf.error_ = err;
        return mf;
    }
    mf.buffer_ = std::move(buf);
    mf.ok_ = true;
    mf.data_ = mf.buffer_.data();
    mf.size_ = mf.buffer_.size();
    return mf;
#endif
}

void MappedFile::release() noexcept {
#ifdef _WIN32
    if (view_ != nullptr) ::UnmapViewOfFile(view_);
    if (mapping_handle_ != nullptr) ::CloseHandle(mapping_handle_);
    if (file_handle_ != nullptr && file_handle_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(file_handle_);
    }
#endif
    view_ = nullptr;
    mapping_handle_ = nullptr;
    file_handle_ = nullptr;
    data_ = nullptr;
    size_ = 0;
    ok_ = false;
    mapped_ = false;
}

void MappedFile::adopt(MappedFile&& other) noexcept {
    ok_ = other.ok_;
    mapped_ = other.mapped_;
    error_ = std::move(other.error_);
    size_ = other.size_;
    file_handle_ = other.file_handle_;
    mapping_handle_ = other.mapping_handle_;
    view_ = other.view_;
    buffer_ = std::move(other.buffer_);
    // Re-point into whichever storage actually holds the bytes; buffer_ may
    // have moved (small-string optimisation invalidates the old pointer).
    data_ = mapped_ ? static_cast<const char*>(view_) : buffer_.data();

    other.ok_ = false;
    other.mapped_ = false;
    other.file_handle_ = nullptr;
    other.mapping_handle_ = nullptr;
    other.view_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
}

MappedFile::MappedFile(MappedFile&& other) noexcept { adopt(std::move(other)); }

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        release();
        adopt(std::move(other));
    }
    return *this;
}

MappedFile::~MappedFile() { release(); }

} // namespace la
