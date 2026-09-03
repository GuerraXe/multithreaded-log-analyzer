#pragma once

#include "parse/log_record.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace la {

// Declarative filter criteria, populated by the CLI. Every field is optional;
// an all-default spec keeps every record. Categories combine with AND; values
// within a repeatable category combine with OR.
struct FilterSpec {
    std::optional<std::int64_t> from_ms;      // keep epoch_ms >= from_ms
    std::optional<std::int64_t> to_ms;        // keep epoch_ms <  to_ms
    std::optional<Level> min_level;           // keep level >= min_level
    std::vector<Level> level_only;            // keep level in set (overrides min_level)
    std::vector<std::string> services;        // keep service in set
    std::vector<int> status_classes;          // keep status/100 in set (1..5)
    std::optional<std::string> path_prefix;   // keep path starting with prefix
    std::optional<std::string> path_contains; // keep path containing substring
};

// A compiled, stateless predicate. Construct once, share across worker threads,
// call matches() per record.
class RecordFilter {
public:
    explicit RecordFilter(FilterSpec spec);

    bool matches(const LogRecord& r) const;

    // True when no criteria are set (matches() would always return true).
    bool is_pass_through() const;

private:
    FilterSpec spec_;
    bool level_mask_[kLevelCount] = {};
    bool use_level_mask_ = false;
};

} // namespace la
