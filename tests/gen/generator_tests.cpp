#include "support/test_framework.hpp"

#include "aggregate/aggregate.hpp"
#include "aggregate/aggregator.hpp"
#include "filter/record_filter.hpp"
#include "gen/generator.hpp"
#include "parse/pipe_format.hpp"
#include "parse/timestamp.hpp"

#include <cstdint>
#include <sstream>
#include <string>

using namespace la;

namespace {
std::string gen(std::uint64_t lines, std::uint64_t seed) {
    std::ostringstream os;
    GenOptions o;
    o.lines = lines;
    o.seed = seed;
    generate_log(os, o);
    return os.str();
}

std::size_t count_lines(const std::string& s) {
    std::size_t n = 0;
    for (char c : s) n += (c == '\n');
    return n;
}
} // namespace

TEST_CASE("generator: identical seed and count produce identical bytes") {
    CHECK_EQ(gen(1000, 123), gen(1000, 123));
}

TEST_CASE("generator: different seeds produce different output") {
    CHECK(gen(1000, 1) != gen(1000, 2));
}

TEST_CASE("generator: emits exactly the requested number of lines") {
    CHECK_EQ(count_lines(gen(0, 5)), std::size_t{0});
    CHECK_EQ(count_lines(gen(1, 5)), std::size_t{1});
    CHECK_EQ(count_lines(gen(4321, 5)), std::size_t{4321});
}

TEST_CASE("generator: output parses with no malformed or blank lines") {
    const std::string log = gen(5000, 99);
    const PipeDelimitedFormat fmt;
    const RecordFilter all{FilterSpec{}};
    const Aggregate a = aggregate_buffer(log, fmt, all, 60'000);
    CHECK_EQ(a.records, std::uint64_t{5000});
    CHECK_EQ(a.malformed, std::uint64_t{0});
    CHECK_EQ(a.blank, std::uint64_t{0});
}

TEST_CASE("generator: severity mix is roughly the documented distribution") {
    const std::string log = gen(20000, 7);
    const PipeDelimitedFormat fmt;
    const RecordFilter all{FilterSpec{}};
    const Aggregate a = aggregate_buffer(log, fmt, all, 60'000);

    const double info = static_cast<double>(a.by_level[level_index(Level::Info)]) / 20000.0;
    const double warn = static_cast<double>(a.by_level[level_index(Level::Warn)]) / 20000.0;
    CHECK(info > 0.78 && info < 0.92);      // ~85%
    CHECK(warn > 0.05 && warn < 0.16);      // ~10%
    CHECK(a.by_level[level_index(Level::Error)] > 0);
    CHECK(a.by_level[level_index(Level::Fatal)] > 0);
}

TEST_CASE("generator: timestamps are monotonically non-decreasing") {
    const std::string log = gen(3000, 4);
    std::int64_t prev = INT64_MIN;
    std::size_t start = 0;
    while (start < log.size()) {
        const std::size_t nl = log.find('\n', start);
        const std::string_view line(log.data() + start,
                                    (nl == std::string::npos ? log.size() : nl) - start);
        const std::size_t bar = line.find(" | ");
        const auto ts = parse_timestamp(line.substr(0, bar));
        CHECK(ts.has_value());
        CHECK(*ts >= prev);
        prev = *ts;
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}
