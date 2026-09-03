#include "support/test_framework.hpp"

#include "report/json_renderer.hpp"
#include "stats/report.hpp"

#include <sstream>
#include <string>

using namespace la;

namespace {
bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
} // namespace

TEST_CASE("json_quote: escapes the JSON metacharacters") {
    CHECK_EQ(json_quote("plain"), std::string("\"plain\""));
    CHECK_EQ(json_quote("a\"b"), std::string("\"a\\\"b\""));
    CHECK_EQ(json_quote("a\\b"), std::string("\"a\\\\b\""));
    CHECK_EQ(json_quote("tab\there"), std::string("\"tab\\there\""));
    // "\x07" is written as its own literal so the hex escape does not eat 'e'.
    CHECK_EQ(json_quote(std::string("bell\x07" "end")), std::string("\"bell\\u0007end\""));
}

TEST_CASE("render_json: empty report is well-formed with all top-level keys") {
    Report r;
    std::ostringstream os;
    render_json(r, os);
    const std::string s = os.str();
    CHECK(contains(s, "\"tool\":"));
    CHECK(contains(s, "\"input\":"));
    CHECK(contains(s, "\"severity\":"));
    CHECK(contains(s, "\"status_classes\":"));
    CHECK(contains(s, "\"response_codes\": []"));   // empty array collapses
    CHECK(contains(s, "\"busiest_endpoints\": []"));
    CHECK(contains(s, "\"timeline\":"));
    CHECK(contains(s, "\"malformed_samples\": []"));
    CHECK(contains(s, "\"exact_percentiles\": false"));
    // balanced braces
    std::size_t open = 0, close = 0;
    for (char c : s) {
        open += (c == '{');
        close += (c == '}');
    }
    CHECK_EQ(open, close);
}

TEST_CASE("render_json: latency sentinels become null") {
    Report r;
    EndpointRow row;
    row.endpoint = "GET /x";
    row.count = 1;
    row.timed = 1;
    row.p50_ms = 5;
    row.p90_ms = kNoValueMs;
    row.p99_ms = kOverflowMs;
    r.busiest_endpoints.push_back(row);

    std::ostringstream os;
    render_json(r, os);
    const std::string s = os.str();
    CHECK(contains(s, "\"p50_ms\": 5"));
    CHECK(contains(s, "\"p90_ms\": null"));
    CHECK(contains(s, "\"p99_ms\": null"));
}
