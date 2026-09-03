#pragma once

#include <cstddef>
#include <string_view>

namespace la {

// Invoke `fn(std::string_view)` once per line in `buf`, splitting on '\n'.
// A trailing '\n' does not yield a final empty line; a missing trailing '\n'
// still yields the last line. Header-only: shared by the analyze and
// aggregate drivers (and, from M5, by the memory-mapped chunk iterator).
template <typename F>
void for_each_line(std::string_view buf, F&& fn) {
    std::size_t start = 0;
    while (start < buf.size()) {
        const std::size_t nl = buf.find('\n', start);
        if (nl == std::string_view::npos) {
            fn(buf.substr(start));
            return;
        }
        fn(buf.substr(start, nl - start));
        start = nl + 1;
    }
}

} // namespace la
