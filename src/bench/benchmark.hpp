#pragma once

#include "aggregate/aggregator.hpp"
#include "filter/record_filter.hpp"
#include "parse/log_format.hpp"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace la {

struct BenchmarkOptions {
    std::vector<unsigned> thread_counts; // empty => {1, 2, 4, 8, hardware_concurrency}
    int repeat = 5;
    int warmup = 1;
    AggregateOptions aggregate;
};

struct BenchRow {
    unsigned threads = 0;
    double median_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double records_per_s = 0.0;
    double mb_per_s = 0.0;
    double speedup = 0.0;    // baseline median / this median
    double efficiency = 0.0; // speedup / threads
};

struct BenchmarkReport {
    std::vector<BenchRow> rows;
    std::uint64_t records = 0;
    std::uint64_t kept = 0;
    std::uint64_t malformed = 0;
    std::uint64_t bytes = 0;
    int repeat = 0;
    int warmup = 0;
    unsigned baseline_threads = 1;
    std::optional<std::uint64_t> peak_working_set_bytes;
    std::string verdict;
};

// Time parallel_aggregate over `buffer` for each requested thread count
// (warmup runs discarded, then `repeat` measured runs; median reported).
// Pure with respect to I/O; does not read files or write output.
BenchmarkReport run_benchmark_core(std::string_view buffer, const ILogFormat& fmt,
                                   const RecordFilter& filter, const BenchmarkOptions& opt);

void render_benchmark_text(const BenchmarkReport& r, std::ostream& os);
void render_benchmark_json(const BenchmarkReport& r, std::ostream& os);

} // namespace la
