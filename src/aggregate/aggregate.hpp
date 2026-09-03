#pragma once

#include "aggregate/endpoint_stat.hpp"
#include "aggregate/time_buckets.hpp"
#include "parse/log_record.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace la {

// A record counts as a failure when its severity is Error/Fatal or its HTTP
// status is 5xx.
bool is_failure(const LogRecord& r);

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

    explicit Aggregate(std::int64_t interval_ms = 60'000) : time_buckets(interval_ms) {}

    // Fold one kept record into the accumulator.
    void observe(const LogRecord& r);

    // Combine another accumulator into this one.
    void merge(const Aggregate& other);
};

} // namespace la
