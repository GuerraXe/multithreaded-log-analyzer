#include "support/test_framework.hpp"

#include "report/text_renderer.hpp"
#include "stats/report.hpp"

#include <sstream>
#include <string>

using namespace la;

namespace {
bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
} // namespace

TEST_CASE("format_interval: seconds / minutes / hours") {
    CHECK_EQ(format_interval(30'000), std::string("30s"));
    CHECK_EQ(format_interval(60'000), std::string("1m"));
    CHECK_EQ(format_interval(300'000), std::string("5m"));
    CHECK_EQ(format_interval(3'600'000), std::string("1h"));
    CHECK_EQ(format_interval(90'000), std::string("90s"));
}

TEST_CASE("render_text: empty report still has every section") {
    Report r;
    std::ostringstream os;
    render_text(r, os);
    const std::string s = os.str();
    CHECK(contains(s, "input"));
    CHECK(contains(s, "severity"));
    CHECK(contains(s, "response codes"));
    CHECK(contains(s, "top services"));
    CHECK(contains(s, "top errors"));
    CHECK(contains(s, "busiest endpoints"));
    CHECK(contains(s, "slowest endpoints"));
    CHECK(contains(s, "traffic (1m buckets)"));
    CHECK(contains(s, "(none)")); // empty lists render a placeholder
}

TEST_CASE("render_text: latency sentinels") {
    Report r;
    EndpointRow row;
    row.endpoint = "GET /x";
    row.count = 1;
    row.timed = 1;
    row.mean_ms = 5.0;
    row.p50_ms = 5;
    row.p90_ms = 10;
    row.p99_ms = kOverflowMs;
    row.max_ms = 42;
    r.busiest_endpoints.push_back(row);

    std::ostringstream os;
    render_text(r, os);
    const std::string s = os.str();
    CHECK(contains(s, "GET /x"));
    CHECK(contains(s, "mean=5.0ms"));
    CHECK(contains(s, "p99=>10000")); // overflow sentinel
    CHECK(contains(s, "max=42"));
}
