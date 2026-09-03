#include "parse/log_format.hpp"

namespace la {

std::string_view reason_code(ParseError e) {
    switch (e) {
        case ParseError::None: return "none";
        case ParseError::FieldCount: return "field_count";
        case ParseError::Timestamp: return "timestamp";
        case ParseError::Level: return "level";
        case ParseError::Service: return "service";
        case ParseError::Request: return "request";
        case ParseError::Status: return "status";
        case ParseError::Duration: return "duration";
    }
    return "none";
}

bool is_blank_line(std::string_view line) {
    for (char c : line) {
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\v':
            case '\f':
                continue;
            default:
                return false;
        }
    }
    return true;
}

} // namespace la
