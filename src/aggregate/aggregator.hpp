#pragma once

#include "aggregate/aggregate.hpp"
#include "filter/record_filter.hpp"
#include "parse/log_format.hpp"

#include <cstdint>
#include <string_view>

namespace la {

// Single-threaded aggregation over an in-memory buffer: split on '\n', parse,
// filter, and fold each kept record into an Aggregate. Pure. This is the
// sequential reference against which the M6 parallel path is checked for
// equivalence.
Aggregate aggregate_buffer(std::string_view buffer, const ILogFormat& fmt,
                           const RecordFilter& filter, std::int64_t interval_ms);

} // namespace la
