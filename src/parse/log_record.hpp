#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace la {

// Severity levels, ordered least to most severe. The numeric values are the
// canonical index used by by-level count arrays and by the `--level` threshold
// filter, so their order must not change.
enum class Level : std::uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5,
};

inline constexpr int kLevelCount = 6;

// Case-insensitive parse of a level name ("info", "INFO", "InFo" all match).
std::optional<Level> parse_level(std::string_view s);

// Canonical upper-case name ("TRACE".."FATAL").
std::string_view to_string(Level lv);

// 0..5, matching the enum value; convenience for indexing count arrays.
constexpr int level_index(Level lv) { return static_cast<int>(lv); }

// HTTP request methods. `None` marks a log line that carries no request.
enum class Method : std::uint8_t {
    None = 0,
    Get,
    Post,
    Put,
    Patch,
    Delete,
    Head,
    Options,
};

// Exact, case-sensitive parse of an upper-case method token ("GET", "POST", ...).
std::optional<Method> parse_method(std::string_view s);

// Canonical name ("GET", ...); `None` maps to the empty string.
std::string_view to_string(Method m);

// One parsed log line. String fields are views into the caller's input buffer
// and are valid only for that buffer's lifetime.
struct LogRecord {
    std::int64_t epoch_ms = 0;        // UTC milliseconds since the Unix epoch
    Level level = Level::Info;
    std::string_view service;
    Method method = Method::None;
    std::string_view path;            // empty when method == None
    std::uint16_t status = 0;         // 0 means "no status"
    std::int64_t duration_us = -1;    // -1 means "no duration"; else microseconds
    std::string_view message;
};

} // namespace la
