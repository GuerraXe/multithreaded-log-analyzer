#include "parse/log_record.hpp"

#include <cstddef>

namespace la {
namespace {

// Case-insensitive equality where `upper` is a known upper-case ASCII literal.
bool ieq_upper(std::string_view s, std::string_view upper) {
    if (s.size() != upper.size()) return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c != upper[i]) return false;
    }
    return true;
}

} // namespace

std::optional<Level> parse_level(std::string_view s) {
    if (ieq_upper(s, "TRACE")) return Level::Trace;
    if (ieq_upper(s, "DEBUG")) return Level::Debug;
    if (ieq_upper(s, "INFO")) return Level::Info;
    if (ieq_upper(s, "WARN")) return Level::Warn;
    if (ieq_upper(s, "ERROR")) return Level::Error;
    if (ieq_upper(s, "FATAL")) return Level::Fatal;
    return std::nullopt;
}

std::string_view to_string(Level lv) {
    switch (lv) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "INFO";
}

std::optional<Method> parse_method(std::string_view s) {
    if (s == "GET") return Method::Get;
    if (s == "POST") return Method::Post;
    if (s == "PUT") return Method::Put;
    if (s == "PATCH") return Method::Patch;
    if (s == "DELETE") return Method::Delete;
    if (s == "HEAD") return Method::Head;
    if (s == "OPTIONS") return Method::Options;
    return std::nullopt;
}

std::string_view to_string(Method m) {
    switch (m) {
        case Method::None: return "";
        case Method::Get: return "GET";
        case Method::Post: return "POST";
        case Method::Put: return "PUT";
        case Method::Patch: return "PATCH";
        case Method::Delete: return "DELETE";
        case Method::Head: return "HEAD";
        case Method::Options: return "OPTIONS";
    }
    return "";
}

} // namespace la
