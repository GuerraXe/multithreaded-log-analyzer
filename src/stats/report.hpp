#pragma once

#include "aggregate/aggregate.hpp"
#include "parse/log_record.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace la {

// A ranked (label, value) pair.
struct CountRow {
    std::string key;
    std::uint64_t value = 0;
};

// A latency value in milliseconds carries two sentinels:
//   -1          => no timed samples
//   INT64_MAX   => percentile fell in the ">10s" overflow bucket
inline constexpr std::int64_t kNoValueMs = -1;
inline constexpr std::int64_t kOverflowMs = INT64_MAX;

struct EndpointRow {
    std::string endpoint; // "METHOD /path"
    std::uint64_t count = 0;
    std::uint64_t timed = 0;
    double mean_ms = 0.0;
    double stddev_ms = 0.0;
    std::int64_t min_ms = kNoValueMs;
    std::int64_t max_ms = kNoValueMs;
    std::int64_t p50_ms = kNoValueMs;
    std::int64_t p90_ms = kNoValueMs;
    std::int64_t p99_ms = kNoValueMs;
};

struct TimeRow {
    std::int64_t bucket_start_ms = 0;
    std::uint64_t requests = 0;
    std::uint64_t errors = 0;
};

// The fully-computed, render-ready analysis result. Pure data: no formatting
// decisions, no I/O. Every ranked list is already sorted with the SPEC's total
// order (value descending, then key ascending) so output does not depend on
// hash-map iteration order or thread count.
struct Report {
    std::uint64_t total_lines = 0;
    std::uint64_t records = 0;
    std::uint64_t kept = 0;
    std::uint64_t malformed = 0;
    std::uint64_t blank = 0;
    std::uint64_t bytes = 0;

    std::array<std::uint64_t, kLevelCount> by_level{};
    std::uint64_t errors = 0;   // Error + Fatal
    std::uint64_t warnings = 0; // Warn
    std::array<std::uint64_t, 6> by_status_class{}; // index 1..5

    std::vector<CountRow> status_codes;         // every code, ascending
    std::vector<CountRow> top_status_codes;     // by count desc
    std::vector<CountRow> top_services;         // by count desc
    std::vector<CountRow> top_errors;           // by count desc
    std::vector<CountRow> top_failure_services; // by count desc
    std::vector<EndpointRow> busiest_endpoints; // by count desc
    std::vector<EndpointRow> slowest_endpoints; // by mean desc, timed >= 1
    std::vector<TimeRow> timeline;              // ascending by bucket start

    std::int64_t interval_ms = 60'000;
    int top_n = 10;
};

Report build_report(const Aggregate& agg, int top_n);

} // namespace la
