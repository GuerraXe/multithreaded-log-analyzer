#include "support/test_framework.hpp"

#include "aggregate/aggregate.hpp"
#include "aggregate/aggregator.hpp"
#include "filter/record_filter.hpp"
#include "parse/pipe_format.hpp"
#include "stats/report.hpp"

#include <cstdint>
#include <string>

using namespace la;

namespace {

const PipeDelimitedFormat kFmt;
const RecordFilter kAll{FilterSpec{}};

const char* const kLog =
    "2026-01-01T00:00:00Z | INFO | api | GET /a | 200 | 10.0 | ok\n"
    "2026-01-01T00:00:01Z | INFO | api | GET /a | 200 | 30.0 | ok\n"
    "2026-01-01T00:00:02Z | INFO | api | GET /a | 200 | 50.0 | ok\n"
    "2026-01-01T00:00:03Z | INFO | web | GET /b | 200 | 5.0 | ok\n"
    "2026-01-01T00:00:04Z | ERROR | api | GET /a | 500 | 200.0 | kaboom\n"
    "2026-01-01T00:00:05Z | ERROR | web | GET /b | 500 | 1.0 | kaboom\n"
    "2026-01-01T00:00:06Z | ERROR | web | GET /b | 500 | 2.0 | other\n";

Report make() {
    const Aggregate a = aggregate_buffer(kLog, kFmt, kAll, 60'000);
    return build_report(a, 10);
}

const CountRow* find(const std::vector<CountRow>& v, const std::string& k) {
    for (const auto& c : v) {
        if (c.key == k) return &c;
    }
    return nullptr;
}

} // namespace

TEST_CASE("report: totals and severity rollup") {
    const Report r = make();
    CHECK_EQ(r.records, std::uint64_t{7});
    CHECK_EQ(r.kept, std::uint64_t{7});
    CHECK_EQ(r.errors, std::uint64_t{3});
    CHECK_EQ(r.warnings, std::uint64_t{0});
    CHECK_EQ(r.by_status_class[2], std::uint64_t{4});
    CHECK_EQ(r.by_status_class[5], std::uint64_t{3});
}

TEST_CASE("report: response codes ascending; top codes by count") {
    const Report r = make();
    CHECK_EQ(r.status_codes.size(), std::size_t{2});
    CHECK_EQ(r.status_codes[0].key, std::string("200")); // ascending
    CHECK_EQ(r.status_codes[1].key, std::string("500"));
    CHECK_EQ(r.top_status_codes[0].key, std::string("200")); // 4 beats 3
    CHECK_EQ(r.top_status_codes[0].value, std::uint64_t{4});
}

TEST_CASE("report: top services and errors ranked by count then key") {
    const Report r = make();
    CHECK_EQ(r.top_services[0].key, std::string("api")); // 4
    CHECK_EQ(r.top_services[1].key, std::string("web")); // 3
    CHECK_EQ(find(r.top_errors, "kaboom")->value, std::uint64_t{2});
    CHECK_EQ(find(r.top_errors, "other")->value, std::uint64_t{1});
    CHECK_EQ(r.top_errors[0].key, std::string("kaboom")); // 2 before 1
}

TEST_CASE("report: failures by service") {
    const Report r = make();
    CHECK_EQ(find(r.top_failure_services, "web")->value, std::uint64_t{2});
    CHECK_EQ(find(r.top_failure_services, "api")->value, std::uint64_t{1});
}

TEST_CASE("report: busiest vs slowest endpoint ordering differ") {
    const Report r = make();
    // GET /a: 4 requests; GET /b: 3 requests -> busiest starts with GET /a
    CHECK_EQ(r.busiest_endpoints[0].endpoint, std::string("GET /a"));
    CHECK_EQ(r.busiest_endpoints[0].count, std::uint64_t{4});
    // GET /a mean = (10+30+50+200)/4 = 72.5ms; GET /b mean = (5+1+2)/3 ~ 2.67ms
    CHECK_EQ(r.slowest_endpoints[0].endpoint, std::string("GET /a"));
    CHECK(r.slowest_endpoints[0].mean_ms > r.slowest_endpoints[1].mean_ms);
}

TEST_CASE("report: ranked lists honour top_n") {
    const Aggregate a = aggregate_buffer(kLog, kFmt, kAll, 60'000);
    const Report r = build_report(a, 1);
    CHECK_EQ(r.top_services.size(), std::size_t{1});
    CHECK_EQ(r.busiest_endpoints.size(), std::size_t{1});
    CHECK_EQ(r.top_errors.size(), std::size_t{1});
}

TEST_CASE("report: timeline is chronological") {
    const Report r = make();
    CHECK_EQ(r.timeline.size(), std::size_t{1}); // all within one minute
    CHECK_EQ(r.timeline[0].requests, std::uint64_t{7});
    CHECK_EQ(r.timeline[0].errors, std::uint64_t{3});
}

TEST_CASE("report: exact percentiles use real values, not histogram edges") {
    AggregateOptions opt;
    opt.collect_durations = true;
    const Aggregate a = aggregate_buffer(kLog, kFmt, kAll, opt);

    const Report edges = build_report(a, 10, /*exact=*/false);
    const Report exact = build_report(a, 10, /*exact=*/true);

    // GET /a durations: 10, 30, 50, 200 ms.
    const EndpointRow* e_edges = nullptr;
    const EndpointRow* e_exact = nullptr;
    for (const auto& e : edges.busiest_endpoints)
        if (e.endpoint == "GET /a") e_edges = &e;
    for (const auto& e : exact.busiest_endpoints)
        if (e.endpoint == "GET /a") e_exact = &e;
    CHECK(e_edges != nullptr);
    CHECK(e_exact != nullptr);

    // Histogram p50 rounds up to a bucket edge (50ms); exact p50 is the
    // 2nd-of-4 sample = 30ms.
    CHECK_EQ(e_edges->p50_ms, std::int64_t{50});
    CHECK_EQ(e_exact->p50_ms, std::int64_t{30});
    CHECK_EQ(e_exact->p99_ms, std::int64_t{200}); // top sample
}

TEST_CASE("report: malformed samples pass through with reason slugs") {
    std::string buf = kLog;
    buf += "totally broken line\n";
    AggregateOptions opt;
    const Aggregate a = aggregate_buffer(buf, kFmt, kAll, opt);
    const Report r = build_report(a, 10);
    CHECK_EQ(r.malformed_samples.size(), std::size_t{1});
    CHECK_EQ(r.malformed_samples[0].reason, std::string("field_count"));
    CHECK(r.malformed_samples[0].line > 0);
}
