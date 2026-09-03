#include "io/chunking.hpp"

namespace la {

std::vector<std::string_view> split_into_chunks(std::string_view buffer, std::size_t n) {
    std::vector<std::string_view> chunks;
    if (buffer.empty()) return chunks;
    if (n < 1) n = 1;

    const std::size_t size = buffer.size();
    std::size_t pos = 0;

    for (std::size_t i = 1; i < n && pos < size; ++i) {
        const std::size_t target = (i * size) / n;
        if (target <= pos) continue;

        const std::size_t nl = buffer.find('\n', target);
        const std::size_t end = (nl == std::string_view::npos) ? size : nl + 1;
        if (end <= pos) continue; // no newline between pos and here

        chunks.push_back(buffer.substr(pos, end - pos));
        pos = end;
    }

    if (pos < size) chunks.push_back(buffer.substr(pos));
    return chunks;
}

} // namespace la
