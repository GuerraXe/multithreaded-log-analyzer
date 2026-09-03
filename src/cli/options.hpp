#pragma once

#include "filter/record_filter.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace la {

enum class Command {
    None,
    Analyze,
    Benchmark,
    Gen,
    Version,
    Help,
};

enum class ReportFormat {
    Text,
    Json,
};

// Fully parsed command line. Fields not relevant to the chosen command keep
// their defaults. Benchmark- and gen-specific fields are parsed from M2 on but
// only consumed once those commands are implemented (M7).
struct Options {
    Command command = Command::None;
    std::string input_path;
    std::string format = "pipe";

    int threads = 1; // 0 => std::thread::hardware_concurrency()
    FilterSpec filter;
    int top_n = 10;
    std::int64_t interval_ms = 60'000;
    bool exact_percentiles = false;
    ReportFormat report = ReportFormat::Text;
    bool strict = false;
    int show_malformed = 5;
    std::string output_path; // empty => stdout

    // benchmark
    std::vector<int> threads_list;
    int repeat = 5;
    int warmup = 1;

    // gen
    std::uint64_t gen_lines = 100'000;
    std::uint64_t gen_seed = 1;

    bool quiet = false;
    bool no_color = false;
    std::string help_topic;
};

} // namespace la
