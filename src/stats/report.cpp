#include "stats/report.hpp"

#include "parse/log_format.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace la {
namespace {

std::int64_t us_to_ms_round(std::int64_t us) {
    return us >= 0 ? (us + 500) / 1000 : -((-us + 500) / 1000);
}

// Convert a histogram bucket edge (microseconds, or INT64_MAX) to ms.
std::int64_t edge_to_ms(std::int64_t edge_us) {
    if (edge_us == INT64_MAX) return kOverflowMs;
    if (edge_us < 0) return kNoValueMs;
    return edge_us / 1000;
}

void sort_counts(std::vector<CountRow>& v) {
    std::sort(v.begin(), v.end(), [](const CountRow& a, const CountRow& b) {
        if (a.value != b.value) return a.value > b.value;
        return a.key < b.key;
    });
}

void clamp(std::vector<CountRow>& v, int n) {
    if (n >= 0 && v.size() > static_cast<std::size_t>(n)) v.resize(static_cast<std::size_t>(n));
}

template <class Map>
std::vector<CountRow> ranked(const Map& m, int n) {
    std::vector<CountRow> v;
    v.reserve(m.size());
    for (const auto& [k, val] : m) v.push_back({std::string(k), val});
    sort_counts(v);
    clamp(v, n);
    return v;
}

// Nearest-rank percentile over a sorted ascending sample vector, in
// microseconds. Matches the histogram's rank convention: the p-th percentile
// is the ceil(p/100 * n)-th sample.
std::int64_t exact_percentile_us(const std::vector<std::int64_t>& sorted, double p) {
    if (sorted.empty()) return -1;
    const double n = static_cast<double>(sorted.size());
    std::size_t rank = static_cast<std::size_t>(std::ceil((p / 100.0) * n));
    if (rank == 0) rank = 1;
    if (rank > sorted.size()) rank = sorted.size();
    return sorted[rank - 1];
}

EndpointRow to_row(const std::string& endpoint, const EndpointStat& st, bool exact) {
    EndpointRow r;
    r.endpoint = endpoint;
    r.count = st.count;
    r.timed = st.timed;
    if (st.timed == 0) return r;

    r.mean_ms = st.mean_us() / 1000.0;
    r.stddev_ms = st.stddev_us() / 1000.0;
    r.min_ms = us_to_ms_round(st.min_us);
    r.max_ms = us_to_ms_round(st.max_us);

    if (exact && !st.samples.empty()) {
        std::vector<std::int64_t> sorted = st.samples;
        std::sort(sorted.begin(), sorted.end());
        r.p50_ms = us_to_ms_round(exact_percentile_us(sorted, 50.0));
        r.p90_ms = us_to_ms_round(exact_percentile_us(sorted, 90.0));
        r.p99_ms = us_to_ms_round(exact_percentile_us(sorted, 99.0));
    } else {
        r.p50_ms = edge_to_ms(st.hist.percentile_us(50.0));
        r.p90_ms = edge_to_ms(st.hist.percentile_us(90.0));
        r.p99_ms = edge_to_ms(st.hist.percentile_us(99.0));
    }
    return r;
}

} // namespace

Report build_report(const Aggregate& agg, int top_n, bool exact_percentiles) {
    Report rep;
    rep.top_n = top_n;
    rep.exact_percentiles = exact_percentiles;
    rep.interval_ms = agg.time_buckets.interval_ms();

    rep.total_lines = agg.total_lines;
    rep.records = agg.records;
    rep.kept = agg.kept;
    rep.malformed = agg.malformed;
    rep.blank = agg.blank;
    rep.bytes = agg.bytes;

    rep.by_level = agg.by_level;
    rep.errors = agg.by_level[static_cast<std::size_t>(level_index(Level::Error))] +
                 agg.by_level[static_cast<std::size_t>(level_index(Level::Fatal))];
    rep.warnings = agg.by_level[static_cast<std::size_t>(level_index(Level::Warn))];
    rep.by_status_class = agg.by_status_class;

    // Response codes: every code ascending (std::map already), plus a
    // count-ranked top-N view.
    for (const auto& [code, cnt] : agg.by_status) {
        rep.status_codes.push_back({std::to_string(code), cnt});
    }
    rep.top_status_codes = rep.status_codes; // copy, then re-rank
    sort_counts(rep.top_status_codes);
    clamp(rep.top_status_codes, top_n);

    rep.top_services = ranked(agg.by_service, top_n);
    rep.top_errors = ranked(agg.error_messages, top_n);
    rep.top_failure_services = ranked(agg.failures_by_service, top_n);

    // Endpoint rows: build once, then produce two orderings.
    std::vector<EndpointRow> rows;
    rows.reserve(agg.endpoints.size());
    for (const auto& [key, st] : agg.endpoints) rows.push_back(to_row(key, st, exact_percentiles));

    std::vector<EndpointRow> busiest = rows;
    std::sort(busiest.begin(), busiest.end(), [](const EndpointRow& a, const EndpointRow& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.endpoint < b.endpoint;
    });
    if (top_n >= 0 && busiest.size() > static_cast<std::size_t>(top_n)) {
        busiest.resize(static_cast<std::size_t>(top_n));
    }
    rep.busiest_endpoints = std::move(busiest);

    std::vector<EndpointRow> slowest;
    for (const EndpointRow& r : rows) {
        if (r.timed >= 1) slowest.push_back(r);
    }
    std::sort(slowest.begin(), slowest.end(), [](const EndpointRow& a, const EndpointRow& b) {
        if (a.mean_ms != b.mean_ms) return a.mean_ms > b.mean_ms;
        return a.endpoint < b.endpoint;
    });
    if (top_n >= 0 && slowest.size() > static_cast<std::size_t>(top_n)) {
        slowest.resize(static_cast<std::size_t>(top_n));
    }
    rep.slowest_endpoints = std::move(slowest);

    for (const auto& [start, counts] : agg.time_buckets.buckets()) {
        rep.timeline.push_back({start, counts.requests, counts.errors});
    }

    for (const MalformedSample& m : agg.malformed_samples) {
        rep.malformed_samples.push_back({m.line, std::string(reason_code(m.reason)), m.text});
    }

    return rep;
}

} // namespace la
