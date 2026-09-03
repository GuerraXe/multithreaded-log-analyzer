#include "support/test_framework.hpp"

#include "aggregate/aggregate.hpp"
#include "aggregate/aggregator.hpp"
#include "filter/record_filter.hpp"
#include "parse/pipe_format.hpp"

#include <cstdint>
#include <string>

using namespace la;

namespace {

const PipeDelimitedFormat kFmt;
const RecordFilter kAll{FilterSpec{}};

// timestamp | level | service | request | status | duration_ms | message
// (empty request/status/duration fields are written as "  " between bars)
const char* const kLog =
    "2026-01-01T00:00:00Z | INFO | api | GET /v1/users | 200 | 12.0 | ok\n"
    "2026-01-01T00:00:10Z | INFO | api | GET /v1/users | 200 | 8.0 | ok\n"
    "2026-01-01T00:00:20Z | ERROR | api | POST /v1/checkout | 500 | 40.0 | boom\n"
    "2026-01-01T00:01:00Z | ERROR | api | POST /v1/checkout | 500 | 60.0 | boom\n"
    "2026-01-01T00:01:30Z | WARN | db |  |  |  | slow query\n"
    "2026-01-01T00:02:00Z | FATAL | db |  |  |  | disk full\n"
    "garbage line here\n"
    "\n"
    "2026-01-01T00:02:30Z | INFO | cache | GET /health | 204 | 1.0 | ok\n";

Aggregate whole() { return aggregate_buffer(kLog, kFmt, kAll, 60'000); }

} // namespace

TEST_CASE("aggregate: line-level totals") {
    const Aggregate a = whole();
    CHECK_EQ(a.blank, std::uint64_t{1});
    CHECK_EQ(a.malformed, std::uint64_t{1});
    CHECK_EQ(a.total_lines, std::uint64_t{8}); // 7 valid + 1 malformed
    CHECK_EQ(a.records, std::uint64_t{7});
    CHECK_EQ(a.kept, std::uint64_t{7});
}

TEST_CASE("aggregate: counts by level and status") {
    const Aggregate a = whole();
    CHECK_EQ(a.by_level[level_index(Level::Info)], std::uint64_t{3});
    CHECK_EQ(a.by_level[level_index(Level::Warn)], std::uint64_t{1});
    CHECK_EQ(a.by_level[level_index(Level::Error)], std::uint64_t{2});
    CHECK_EQ(a.by_level[level_index(Level::Fatal)], std::uint64_t{1});
    CHECK_EQ(a.by_status.at(200), std::uint64_t{2});
    CHECK_EQ(a.by_status.at(500), std::uint64_t{2});
    CHECK_EQ(a.by_status.at(204), std::uint64_t{1});
    CHECK_EQ(a.by_status_class[2], std::uint64_t{3});
    CHECK_EQ(a.by_status_class[5], std::uint64_t{2});
}

TEST_CASE("aggregate: services, error messages and failures") {
    const Aggregate a = whole();
    CHECK_EQ(a.by_service.at("api"), std::uint64_t{4});
    CHECK_EQ(a.by_service.at("db"), std::uint64_t{2});
    CHECK_EQ(a.by_service.at("cache"), std::uint64_t{1});

    // error_messages only for level >= ERROR
    CHECK_EQ(a.error_messages.at("boom"), std::uint64_t{2});
    CHECK_EQ(a.error_messages.at("disk full"), std::uint64_t{1});
    CHECK(a.error_messages.find("slow query") == a.error_messages.end());

    // failures: level >= ERROR or status 5xx
    CHECK_EQ(a.failures_by_service.at("api"), std::uint64_t{2});
    CHECK_EQ(a.failures_by_service.at("db"), std::uint64_t{1}); // FATAL only
}

TEST_CASE("aggregate: endpoint latency stats") {
    const Aggregate a = whole();
    const EndpointStat& users = a.endpoints.at("GET /v1/users");
    CHECK_EQ(users.count, std::uint64_t{2});
    CHECK_EQ(users.timed, std::uint64_t{2});
    CHECK_EQ(users.sum_us, std::int64_t{20'000}); // 12ms + 8ms
    CHECK_EQ(users.min_us, std::int64_t{8'000});
    CHECK_EQ(users.max_us, std::int64_t{12'000});

    const EndpointStat& checkout = a.endpoints.at("POST /v1/checkout");
    CHECK_EQ(checkout.sum_us, std::int64_t{100'000}); // 40 + 60
}

TEST_CASE("aggregate: time buckets") {
    const Aggregate a = whole();
    const auto& b = a.time_buckets.buckets();
    // minute 0: 3 requests (2 INFO + 1 ERROR), 1 error
    const std::int64_t m0 = 1767225600000; // 2026-01-01T00:00:00Z
    CHECK_EQ(b.at(m0).requests, std::uint64_t{3});
    CHECK_EQ(b.at(m0).errors, std::uint64_t{1});
}

TEST_CASE("aggregate: malformed samples carry line number and reason, bounded") {
    std::string buf;
    buf += "2026-01-01T00:00:00Z | INFO | api | GET /a | 200 | 1.0 | ok\n"; // line 1 ok
    buf += "bad one\n";                                                       // line 2
    buf += "2026-01-01T00:00:01Z | NOPE | api |  |  |  | x\n";                // line 3 level
    buf += "2026-01-01T00:00:02Z | INFO | api |  | 999 |  | x\n";             // line 4 status
    buf += "also bad\n";                                                      // line 5

    AggregateOptions opt;
    opt.malformed_sample_limit = 2;
    const Aggregate a = aggregate_buffer(buf, kFmt, kAll, opt);

    CHECK_EQ(a.malformed, std::uint64_t{4});         // count is exact
    CHECK_EQ(a.malformed_samples.size(), std::size_t{2}); // samples are bounded
    CHECK_EQ(a.malformed_samples[0].line, std::uint64_t{2});
    CHECK(a.malformed_samples[0].reason == ParseError::FieldCount);
    CHECK_EQ(a.malformed_samples[1].line, std::uint64_t{3});
    CHECK(a.malformed_samples[1].reason == ParseError::Level);
}

TEST_CASE("aggregate: collect_durations retains per-endpoint samples") {
    std::string buf;
    buf += "2026-01-01T00:00:00Z | INFO | api | GET /a | 200 | 10.0 | ok\n";
    buf += "2026-01-01T00:00:01Z | INFO | api | GET /a | 200 | 20.0 | ok\n";

    AggregateOptions opt;
    opt.collect_durations = true;
    const Aggregate a = aggregate_buffer(buf, kFmt, kAll, opt);
    const auto& s = a.endpoints.at("GET /a").samples;
    CHECK_EQ(s.size(), std::size_t{2});
    CHECK_EQ(s[0], std::int64_t{10'000});
    CHECK_EQ(s[1], std::int64_t{20'000});
}

TEST_CASE("aggregate: malformed sample merge keeps earliest lines, sorted") {
    Aggregate a(60'000, 3);
    a.note_malformed(10, ParseError::Timestamp, "ten");
    a.note_malformed(30, ParseError::Level, "thirty");

    Aggregate b(60'000, 3);
    b.note_malformed(5, ParseError::Status, "five");
    b.note_malformed(20, ParseError::Request, "twenty");

    a.merge(b);
    CHECK_EQ(a.malformed, std::uint64_t{4});
    CHECK_EQ(a.malformed_samples.size(), std::size_t{3});
    CHECK_EQ(a.malformed_samples[0].line, std::uint64_t{5});
    CHECK_EQ(a.malformed_samples[1].line, std::uint64_t{10});
    CHECK_EQ(a.malformed_samples[2].line, std::uint64_t{20});
}

TEST_CASE("aggregate: merge of split halves equals whole (order-independent)") {
    const std::string s = kLog;
    // Split on a newline boundary roughly in the middle.
    const std::size_t cut = s.find('\n', s.size() / 2) + 1;
    const std::string_view left(s.data(), cut);
    const std::string_view right(s.data() + cut, s.size() - cut);

    const Aggregate whole_agg = aggregate_buffer(s, kFmt, kAll, 60'000);

    Aggregate lr(60'000);
    lr.merge(aggregate_buffer(left, kFmt, kAll, 60'000));
    lr.merge(aggregate_buffer(right, kFmt, kAll, 60'000));

    Aggregate rl(60'000);
    rl.merge(aggregate_buffer(right, kFmt, kAll, 60'000));
    rl.merge(aggregate_buffer(left, kFmt, kAll, 60'000));

    for (const Aggregate* a : {&lr, &rl}) {
        CHECK_EQ(a->records, whole_agg.records);
        CHECK_EQ(a->kept, whole_agg.kept);
        CHECK_EQ(a->malformed, whole_agg.malformed);
        CHECK_EQ(a->blank, whole_agg.blank);
        CHECK_EQ(a->by_level[level_index(Level::Error)],
                 whole_agg.by_level[level_index(Level::Error)]);
        CHECK_EQ(a->by_service.at("api"), whole_agg.by_service.at("api"));
        CHECK_EQ(a->error_messages.at("boom"), whole_agg.error_messages.at("boom"));
        CHECK_EQ(a->endpoints.at("POST /v1/checkout").sum_us,
                 whole_agg.endpoints.at("POST /v1/checkout").sum_us);
        CHECK_EQ(a->endpoints.at("GET /v1/users").min_us,
                 whole_agg.endpoints.at("GET /v1/users").min_us);
    }
}
