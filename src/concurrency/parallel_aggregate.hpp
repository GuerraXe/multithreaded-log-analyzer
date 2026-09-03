#pragma once

#include "aggregate/aggregate.hpp"
#include "aggregate/aggregator.hpp"
#include "filter/record_filter.hpp"
#include "parse/log_format.hpp"

#include <string_view>

namespace la {

// Multithreaded aggregation. The buffer is split into one newline-aligned
// chunk per worker; each worker fills a private Aggregate over its chunk with
// no shared mutable state and no locks; the main thread folds the partials
// with Aggregate::merge after every worker has joined.
//
// `threads == 0` resolves to std::thread::hardware_concurrency() (min 1).
// `threads <= 1`, or a buffer that splits into a single chunk, runs the
// sequential path directly (no thread is spawned).
//
// The result is logically identical to aggregate_buffer over the whole
// buffer: all counts, sums, min/max, and histogram bins are bit-identical;
// only per-endpoint stddev may differ (double summation order) -- see SPEC
// section 6.
Aggregate parallel_aggregate(std::string_view buffer, const ILogFormat& fmt,
                             const RecordFilter& filter, const AggregateOptions& opt,
                             unsigned threads);

// Resolve a requested thread count to an effective one (0 -> hardware
// concurrency, clamped to >= 1). Exposed for the benchmark harness and tests.
unsigned resolve_thread_count(unsigned requested);

} // namespace la
