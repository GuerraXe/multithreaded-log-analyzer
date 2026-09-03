#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace la {

// Parse an ISO-8601 UTC timestamp of the form
//   YYYY-MM-DDThh:mm:ss[.fff...]Z
// The 'T' date/time separator and trailing 'Z' are required. Fractional
// seconds are optional (one or more digits) and are truncated to whole
// milliseconds. The input must contain nothing before or after the grammar
// (callers trim surrounding whitespace first).
//
// Returns Unix epoch milliseconds (negative for pre-1970 instants), or
// std::nullopt if the input does not match the grammar or encodes an invalid
// calendar date or wall-clock time.
std::optional<std::int64_t> parse_timestamp(std::string_view s);

} // namespace la
