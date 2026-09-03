#include "support/test_framework.hpp"

#include "bench/benchmark.hpp"
#include "filter/record_filter.hpp"
#include "gen/generator.hpp"
#include "parse/pipe_format.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

using namespace la;

namespace {
const PipeDelimitedFormat kFmt;
const RecordFilter kAll{FilterSpec{}};

std::string small_log() {
    std::ostringstream os;
    GenOptions o;
    o.lines = 4000;
    o.seed = 3;
    generate_log(os, o);
    return os.str();
}

bool finite(double d) { return !std::isnan(d) && !std::isinf(d); }
} // namespace

TEST_CASE("benchmark_core: one row per requested thread count, consistent records") {
    const std::string log = small_log();
    BenchmarkOptions opt;
    opt.thread_counts = {1, 2, 3};
    opt.repeat = 2;
    opt.warmup = 0;

    const BenchmarkReport r = run_benchmark_core(log, kFmt, kAll, opt);
    CHECK_EQ(r.rows.size(), std::size_t{3});
    CHECK_EQ(r.rows[0].threads, 1u);
    CHECK_EQ(r.rows[1].threads, 2u);
    CHECK_EQ(r.rows[2].threads, 3u);
    CHECK_EQ(r.records, std::uint64_t{4000});
    CHECK_EQ(r.malformed, std::uint64_t{0});
    CHECK_EQ(r.baseline_threads, 1u);
}

TEST_CASE("benchmark_core: derived metrics are finite and self-consistent") {
    const std::string log = small_log();
    BenchmarkOptions opt;
    opt.thread_counts = {1, 2};
    opt.repeat = 3;
    opt.warmup = 1;

    const BenchmarkReport r = run_benchmark_core(log, kFmt, kAll, opt);
    for (const auto& row : r.rows) {
        CHECK(finite(row.median_ms) && row.median_ms >= 0.0);
        CHECK(row.min_ms <= row.median_ms + 1e-9);
        CHECK(row.max_ms + 1e-9 >= row.median_ms);
        CHECK(finite(row.records_per_s));
        CHECK(finite(row.speedup) && row.speedup > 0.0);
        CHECK(finite(row.efficiency));
    }
    // baseline row has speedup ~ 1.0 by construction
    CHECK(std::fabs(r.rows[0].speedup - 1.0) < 1e-9);
    CHECK(!r.verdict.empty());
}

TEST_CASE("benchmark_core: empty thread list falls back to a default sweep") {
    const std::string log = small_log();
    BenchmarkOptions opt;
    opt.repeat = 1;
    opt.warmup = 0;
    const BenchmarkReport r = run_benchmark_core(log, kFmt, kAll, opt);
    CHECK(!r.rows.empty());
    CHECK_EQ(r.rows.front().threads, 1u); // sorted, always includes 1
}

TEST_CASE("benchmark: JSON render is structurally balanced") {
    const std::string log = small_log();
    BenchmarkOptions opt;
    opt.thread_counts = {1, 2};
    opt.repeat = 1;
    opt.warmup = 0;
    const BenchmarkReport r = run_benchmark_core(log, kFmt, kAll, opt);

    std::ostringstream os;
    render_benchmark_json(r, os);
    const std::string s = os.str();
    CHECK(s.find("\"rows\"") != std::string::npos);
    CHECK(s.find("\"verdict\"") != std::string::npos);
    std::size_t open = 0, close = 0;
    for (char c : s) {
        open += (c == '{');
        close += (c == '}');
    }
    CHECK_EQ(open, close);
}
