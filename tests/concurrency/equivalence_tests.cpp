#include "support/test_framework.hpp"

#include "aggregate/aggregate.hpp"
#include "aggregate/aggregator.hpp"
#include "concurrency/parallel_aggregate.hpp"
#include "filter/record_filter.hpp"
#include "parse/pipe_format.hpp"
#include "parse/timestamp.hpp"
#include "report/json_renderer.hpp"
#include "stats/report.hpp"

#include <cmath>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace la;

namespace {

const PipeDelimitedFormat kFmt;

// Deterministic synthetic log: fixed seed -> identical bytes every run.
// Mixes levels, services, HTTP and non-HTTP lines, fractional-ms durations
// (so latency sums exercise integer-microsecond accumulation), plus a few
// blank and malformed lines.
std::string make_log(std::size_t n_lines, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    const char* levels[] = {"TRACE", "DEBUG", "INFO", "INFO", "INFO", "WARN", "ERROR", "FATAL"};
    const char* services[] = {"api-gateway", "order-service", "auth", "cache", "db", "scheduler"};
    const char* methods[] = {"GET", "GET", "GET", "POST", "PUT", "DELETE"};
    const char* paths[] = {"/v1/users", "/v1/users/42", "/v1/orders", "/v1/checkout", "/health"};
    const int codes[] = {200, 201, 204, 301, 304, 400, 404, 429, 500, 502, 503};
    const char* msgs[] = {"ok", "request completed", "payment declined", "upstream timeout",
                          "cache miss", "slow downstream"};

    std::string out;
    out.reserve(n_lines * 90);
    std::int64_t ts = 1'767'225'600'000; // 2026-01-01T00:00:00Z

    for (std::size_t i = 0; i < n_lines; ++i) {
        const unsigned r = rng() % 100;
        if (r < 3) {
            out += '\n'; // blank line
            continue;
        }
        if (r < 7) {
            out += "not a valid log line ";
            out += std::to_string(i);
            out += '\n';
            continue;
        }

        ts += 1 + static_cast<std::int64_t>(rng() % 4000);
        const char* lvl = levels[rng() % 8];
        const char* svc = services[rng() % 6];
        const bool http = (rng() % 100) < 70;

        std::string req, st, du;
        if (http) {
            req = std::string(methods[rng() % 6]) + ' ' + paths[rng() % 5];
            st = std::to_string(codes[rng() % 11]);
            du = std::to_string(rng() % 5000) + '.' + std::to_string(rng() % 1000);
        }

        out += format_timestamp(ts);
        out += " | ";
        out += lvl;
        out += " | ";
        out += svc;
        out += " | ";
        out += req;
        out += " | ";
        out += st;
        out += " | ";
        out += du;
        out += " | ";
        out += msgs[rng() % 6];
        out += '\n';
    }
    return out;
}

// "" when the two aggregates match on every bit-exact field; otherwise a short
// description of the first mismatch. Per-endpoint stddev is compared with a
// relative tolerance (the one value SPEC section 6 allows to differ).
std::string first_diff(const Aggregate& a, const Aggregate& b) {
    if (a.total_lines != b.total_lines) return "total_lines";
    if (a.records != b.records) return "records";
    if (a.kept != b.kept) return "kept";
    if (a.malformed != b.malformed) return "malformed";
    if (a.blank != b.blank) return "blank";
    if (a.bytes != b.bytes) return "bytes";
    if (a.by_level != b.by_level) return "by_level";
    if (a.by_status_class != b.by_status_class) return "by_status_class";
    if (a.by_status != b.by_status) return "by_status";
    if (a.by_service != b.by_service) return "by_service";
    if (a.error_messages != b.error_messages) return "error_messages";
    if (a.failures_by_service != b.failures_by_service) return "failures_by_service";

    if (a.endpoints.size() != b.endpoints.size()) return "endpoints.size";
    for (const auto& [k, ea] : a.endpoints) {
        const auto it = b.endpoints.find(k);
        if (it == b.endpoints.end()) return "endpoint missing: " + k;
        const EndpointStat& eb = it->second;
        if (ea.count != eb.count) return "endpoint count: " + k;
        if (ea.timed != eb.timed) return "endpoint timed: " + k;
        if (ea.min_us != eb.min_us) return "endpoint min_us: " + k;
        if (ea.max_us != eb.max_us) return "endpoint max_us: " + k;
        if (ea.sum_us != eb.sum_us) return "endpoint sum_us: " + k;
        if (ea.hist.bins() != eb.hist.bins()) return "endpoint hist: " + k;
        const double sa = ea.stddev_us();
        const double sb = eb.stddev_us();
        const double scale = std::max({1.0, std::fabs(sa), std::fabs(sb)});
        if (std::fabs(sa - sb) > 1e-9 * scale) return "endpoint stddev: " + k;
    }

    if (a.time_buckets.buckets() != b.time_buckets.buckets()) return "time_buckets";

    if (a.malformed_samples.size() != b.malformed_samples.size()) return "malformed_samples.size";
    for (std::size_t i = 0; i < a.malformed_samples.size(); ++i) {
        const auto& x = a.malformed_samples[i];
        const auto& y = b.malformed_samples[i];
        if (x.line != y.line || x.reason != y.reason || x.text != y.text) {
            return "malformed_samples[" + std::to_string(i) + "]";
        }
    }
    return "";
}

std::string json_of(const Aggregate& agg) {
    std::ostringstream os;
    render_json(build_report(agg, 10, /*exact=*/false), os);
    return os.str();
}

} // namespace

TEST_CASE("equivalence: parallel == sequential across many thread counts") {
    const std::string log = make_log(4000, 0xC0FFEE);
    const RecordFilter all{FilterSpec{}};
    AggregateOptions opt;
    opt.interval_ms = 60'000;
    opt.malformed_sample_limit = 8;

    const Aggregate seq = aggregate_buffer(log, kFmt, all, opt);
    const std::string seq_json = json_of(seq);

    for (const unsigned t : {1u, 2u, 3u, 4u, 7u, 8u, 16u, 32u, 64u}) {
        const Aggregate par = parallel_aggregate(log, kFmt, all, opt, t);
        const std::string why = first_diff(seq, par);
        if (!why.empty()) {
            throw la::test::Failure{"threads=" + std::to_string(t) + ": " + why};
        }
        if (json_of(par) != seq_json) {
            throw la::test::Failure{"threads=" + std::to_string(t) + ": rendered JSON differs"};
        }
    }
}

TEST_CASE("equivalence: holds with a non-trivial filter") {
    const std::string log = make_log(3000, 0x1234ABCD);
    FilterSpec spec;
    spec.min_level = Level::Warn;
    spec.services = {"api-gateway", "order-service"};
    const RecordFilter filter{spec};
    AggregateOptions opt;

    const Aggregate seq = aggregate_buffer(log, kFmt, filter, opt);
    for (const unsigned t : {2u, 5u, 8u, 13u}) {
        const Aggregate par = parallel_aggregate(log, kFmt, filter, opt, t);
        const std::string why = first_diff(seq, par);
        CHECK(why.empty());
        if (!why.empty()) throw la::test::Failure{"threads=" + std::to_string(t) + ": " + why};
    }
}

TEST_CASE("equivalence: thread count far exceeding the line count") {
    const std::string log = make_log(12, 0x55);
    const RecordFilter all{FilterSpec{}};
    AggregateOptions opt;
    const Aggregate seq = aggregate_buffer(log, kFmt, all, opt);
    for (const unsigned t : {8u, 64u, 256u}) {
        const Aggregate par = parallel_aggregate(log, kFmt, all, opt, t);
        CHECK(first_diff(seq, par).empty());
    }
}

TEST_CASE("equivalence: empty and blank-only buffers") {
    const RecordFilter all{FilterSpec{}};
    AggregateOptions opt;
    for (const std::string_view buf : {std::string_view(""), std::string_view("\n\n\n\n")}) {
        const Aggregate seq = aggregate_buffer(buf, kFmt, all, opt);
        for (const unsigned t : {1u, 4u, 32u}) {
            const Aggregate par = parallel_aggregate(buf, kFmt, all, opt, t);
            CHECK(first_diff(seq, par).empty());
        }
    }
}

TEST_CASE("equivalence: parallel_aggregate is deterministic across repeats") {
    const std::string log = make_log(2500, 0xDECAF);
    const RecordFilter all{FilterSpec{}};
    AggregateOptions opt;
    const Aggregate a = parallel_aggregate(log, kFmt, all, opt, 8);
    const Aggregate b = parallel_aggregate(log, kFmt, all, opt, 8);
    CHECK(first_diff(a, b).empty());
    CHECK_EQ(json_of(a), json_of(b));
}

TEST_CASE("resolve_thread_count: 0 becomes hardware concurrency, else identity") {
    CHECK(resolve_thread_count(0) >= 1u);
    CHECK_EQ(resolve_thread_count(1), 1u);
    CHECK_EQ(resolve_thread_count(9), 9u);
}
