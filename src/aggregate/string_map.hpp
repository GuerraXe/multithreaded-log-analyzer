#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace la {

// Transparent hash so a std::string-keyed map can be looked up with a
// std::string_view without materialising a std::string. This removes a heap
// allocation per record on the aggregation hot path (services, endpoint keys,
// error messages) whenever the key already exists -- the common case.
struct TransparentStringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
    std::size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
    std::size_t operator()(const char* s) const noexcept {
        return std::hash<std::string_view>{}(std::string_view{s});
    }
};

template <class V>
using StringMap =
    std::unordered_map<std::string, V, TransparentStringHash, std::equal_to<>>;

// Add `by` to the entry for `key`, inserting it (allocating the string) only
// on first sight.
template <class V>
void bump(StringMap<V>& m, std::string_view key, V by = 1) {
    if (const auto it = m.find(key); it != m.end()) {
        it->second += by;
    } else {
        m.emplace(std::string(key), by);
    }
}

} // namespace la
