#pragma once

#include "parse/log_format.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace la {

// Line-level tallies for a single analysis pass.
struct AnalyzeSummary {
    std::uint64_t lines = 0;     // non-blank lines examined
    std::uint64_t records = 0;   // lines that parsed successfully
    std::uint64_t malformed = 0; // non-blank lines that failed to parse
    std::uint64_t blank = 0;     // empty / whitespace-only lines
    std::uint64_t bytes = 0;     // size of the scanned buffer
};

// Scan an in-memory buffer, splitting on '\n'. Pure: no file or console I/O.
// A final line without a trailing newline is still processed.
AnalyzeSummary analyze_buffer(std::string_view buffer, const ILogFormat& fmt);

// Read `path` into memory, run analyze_buffer with the default format, and
// write a short summary to `out`. Returns a process exit code:
//   0  success
//   2  the file could not be opened
int run_analyze(const std::string& path, std::ostream& out, std::ostream& err);

} // namespace la
