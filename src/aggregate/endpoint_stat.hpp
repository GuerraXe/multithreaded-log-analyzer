#pragma once

#include "aggregate/histogram.hpp"

#include <cstdint>
#include <vector>

namespace la {

// Per-endpoint request and latency accumulator. Latency is summed in integer
// microseconds so the merged sum is independent of how requests were
// partitioned across threads (SPEC CR-1/CR-2). `sum_sq_us` feeds the standard
// deviation only, which is the one figure allowed to differ between the
// sequential and multithreaded runs (SPEC CR-3).
//
// `samples` is populated only under --exact-percentiles; otherwise the
// histogram alone answers percentile queries.
struct EndpointStat {
    std::uint64_t count = 0; // requests seen (with or without a duration)
    std::uint64_t timed = 0; // requests that carried a duration
    std::int64_t min_us = 0;
    std::int64_t max_us = 0;
    std::int64_t sum_us = 0;
    double sum_sq_us = 0.0;
    LatencyHistogram hist;
    std::vector<std::int64_t> samples;

    // Record one request. `duration_us < 0` means "no duration": the request
    // still counts toward `count` but not toward the latency statistics.
    // When `keep_sample` is true and a duration is present, it is retained in
    // `samples` for exact percentile computation.
    void observe(std::int64_t duration_us, bool keep_sample = false);

    void merge(const EndpointStat& other);

    double mean_us() const;   // 0 when timed == 0
    double stddev_us() const; // population stddev; 0 when timed < 2
};

} // namespace la
