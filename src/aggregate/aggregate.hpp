#pragma once

#include "aggregate/endpoint_stat.hpp"
#include "aggregate/time_buckets.hpp"
#include "parse/log_format.hpp"
#include "parse/log_record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace la {

// A record counts as a failure when its severity is Error/Fatal or its HTTP
// status is 5xx.
bool is_failure(const LogRecord& r);

// A retained example of a line that failed to parse.
struct MalformedSample {
    std::uint64_t line = 0; // 1-based line number in the file
    ParseError reason = ParseError::None;
    std::string text; // raw line, trailing CR removed, truncated to 256 bytes
};

// One accumulator. A worker fills a local Aggregate over its chunk; the main
// thread folds all workers' Aggregates with merge(). Every field's merge is
// addition or an order-independent map union, so merge is associative and
// commutative (SPEC CR-4).
struct Aggregate {
    std::uint64_t total_lines = 0; // non-blank lines examined
    std::uint64_t records = 0;     // parsed successfully
    std::uint64_t kept = 0;        // parsed and passed the filter
    std::uint64_t malformed = 0;
    std::uint64_t blank = 0;
    std::uint64_t bytes = 0;

    std::array<std::uint64_t, kLevelCount> by_level{};
    std::array<std::uint64_t, 6> by_status_class{}; // index 1..5 used
    std::map<std::uint16_t, std::uint64_t> by_status;
    std::unordered_map<std::string, std::uint64_t> by_service;
    std::unordered_map<std::string, std::uint64_t> error_messages;
    std::unordered_map<std::string, EndpointStat> endpoints;
    std::unordered_map<std::string, std::uint64_t> failures_by_service;
    TimeBuckets time_buckets;

    // Bounded, kept sorted by line number ascending.
    std::vector<MalformedSample> malformed_samples;
    std::size_t sample_limit = 5;
    bool collect_durations = false;

    explicit Aggregate(std::int64_t interval_ms = 60'000, std::size_t sample_limit = 5,
                       bool collect_durations = false)
        : time_buckets(interval_ms),
          sample_limit(sample_limit),
          collect_durations(collect_durations) {}

    // Fold one kept record into the accumulator.
    void observe(const LogRecord& r);

    // Record a malformed line (increments `malformed`; keeps a sample while
    // under the limit).
    void note_malformed(std::uint64_t line, ParseError reason, std::string_view raw);

    // Combine another accumulator into this one.
    void merge(const Aggregate& other);
};

} // namespace la
