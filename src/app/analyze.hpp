#pragma once

#include "cli/options.hpp"
#include "filter/record_filter.hpp"
#include "parse/log_format.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace la {

// Line-level tallies for a quick pass (used by lightweight tests and as a
// cross-check on the aggregator's own line counts).
struct AnalyzeSummary {
    std::uint64_t lines = 0;
    std::uint64_t records = 0;
    std::uint64_t kept = 0;
    std::uint64_t malformed = 0;
    std::uint64_t blank = 0;
    std::uint64_t bytes = 0;
};

AnalyzeSummary analyze_buffer(std::string_view buffer, const ILogFormat& fmt,
                              const RecordFilter& filter);

// Full `analyze` command: read the file, aggregate, build the report model,
// and render it (text; JSON from M4) to stdout or `-o` file. Returns the
// process exit code (0 ok, 1 bad option value, 2 I/O error).
int run_analyze(const Options& opt, std::ostream& out, std::ostream& err);

} // namespace la
