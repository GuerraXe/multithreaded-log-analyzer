#pragma once

#include "aggregate/aggregate.hpp"
#include "filter/record_filter.hpp"
#include "parse/log_format.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace la {

struct AggregateOptions {
    std::int64_t interval_ms = 60'000;
    std::size_t malformed_sample_limit = 5;
    bool collect_durations = false; // retain per-request durations (--exact-percentiles)
    // 1-based file line number of this buffer's first line. Workers set this so
    // malformed-sample line numbers are file-global, not chunk-local.
    std::uint64_t line_number_base = 1;
};

// Single-threaded aggregation over an in-memory buffer: split on '\n', parse,
// filter, and fold each kept record into an Aggregate. Pure. This is the
// sequential reference against which the M6 parallel path is checked for
// equivalence.
Aggregate aggregate_buffer(std::string_view buffer, const ILogFormat& fmt,
                           const RecordFilter& filter, const AggregateOptions& opt);

// Convenience overload for the common case (default sample limit, no duration
// retention).
Aggregate aggregate_buffer(std::string_view buffer, const ILogFormat& fmt,
                           const RecordFilter& filter, std::int64_t interval_ms);

} // namespace la
