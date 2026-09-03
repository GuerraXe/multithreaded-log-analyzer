#pragma once

#include "parse/log_record.hpp"

#include <cstdint>
#include <string_view>

namespace la {

// Why a non-blank line failed to parse. `None` accompanies a successful parse.
enum class ParseError : std::uint8_t {
    None = 0,
    FieldCount, // too few "<SP>|<SP>"-separated fields before the message
    Timestamp,
    Level,
    Service,
    Request,
    Status,
    Duration,
};

// Short stable slug for reports / diagnostics ("timestamp", "field_count", ...).
std::string_view reason_code(ParseError e);

struct ParseResult {
    bool ok = false;
    LogRecord record{};
    ParseError error = ParseError::None;

    static ParseResult success(const LogRecord& r) {
        return ParseResult{true, r, ParseError::None};
    }
    static ParseResult failure(ParseError e) {
        return ParseResult{false, LogRecord{}, e};
    }
};

// True when a line is empty or contains only ASCII whitespace. Such lines are
// skipped by the analyzer and counted separately from malformed lines.
bool is_blank_line(std::string_view line);

// A pluggable log-line grammar. Implementations are stateless and thread-safe;
// a single instance is shared across all worker threads.
class ILogFormat {
public:
    virtual ~ILogFormat() = default;

    // Stable identifier used by `--format` and report headers.
    virtual std::string_view name() const = 0;

    // Parse one line. `line` excludes the terminating '\n' but may retain a
    // trailing '\r' from CRLF input. Never throws; a malformed line yields a
    // failure result, not an exception.
    virtual ParseResult parse_line(std::string_view line) const = 0;
};

} // namespace la
