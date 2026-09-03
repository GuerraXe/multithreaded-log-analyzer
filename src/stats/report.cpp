#include "stats/report.hpp"

#include <algorithm>
#include <string>

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

EndpointRow to_row(const std::string& endpoint, const EndpointStat& st) {
    EndpointRow r;
    r.endpoint = endpoint;
    r.count = st.count;
    r.timed = st.timed;
    if (st.timed > 0) {
        r.mean_ms = st.mean_us() / 1000.0;
        r.stddev_ms = st.stddev_us() / 1000.0;
        r.min_ms = us_to_ms_round(st.min_us);
        r.max_ms = us_to_ms_round(st.max_us);
        r.p50_ms = edge_to_ms(st.hist.percentile_us(50.0));
        r.p90_ms = edge_to_ms(st.hist.percentile_us(90.0));
        r.p99_ms = edge_to_ms(st.hist.percentile_us(99.0));
    }
    return r;
}

} // namespace

Report build_report(const Aggregate& agg, int top_n) {
    Report rep;
    rep.top_n = top_n;
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
    for (const auto& [key, st] : agg.endpoints) rows.push_back(to_row(key, st));

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

    return rep;
}

} // namespace la
