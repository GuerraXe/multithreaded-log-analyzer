#include "report/text_renderer.hpp"

#include "core/version.hpp"
#include "parse/log_record.hpp"
#include "parse/timestamp.hpp"

#include <cstdio>
#include <ostream>
#include <string>

namespace la {
namespace {

std::string ms1(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}

// A latency-ms sentinel-aware renderer: "-" for none, ">10000" for overflow.
std::string lat(std::int64_t ms) {
    if (ms == kNoValueMs) return "-";
    if (ms == kOverflowMs) return ">10000";
    return std::to_string(ms);
}

std::string uint_col(std::uint64_t v, std::size_t width) {
    std::string s = std::to_string(v);
    if (s.size() < width) s.insert(s.begin(), width - s.size(), ' ');
    return s;
}

void section(std::ostream& os, const char* title) {
    os << '\n' << title << '\n';
}

} // namespace

std::string format_interval(std::int64_t interval_ms) {
    const std::int64_t secs = interval_ms / 1000;
    if (secs % 3600 == 0) return std::to_string(secs / 3600) + "h";
    if (secs % 60 == 0) return std::to_string(secs / 60) + "m";
    return std::to_string(secs) + "s";
}

void render_text(const Report& r, std::ostream& os) {
    os << version_string() << " \xE2\x80\x94 analysis report\n";

    section(os, "input");
    os << "  bytes                " << r.bytes << "\n"
       << "  lines (non-blank)    " << r.total_lines << "\n"
       << "  parsed records       " << r.records << "\n"
       << "  kept (after filters) " << r.kept << "\n"
       << "  malformed lines      " << r.malformed << "\n"
       << "  blank lines          " << r.blank << "\n";

    section(os, "severity");
    for (int i = 0; i < kLevelCount; ++i) {
        const auto lv = static_cast<Level>(i);
        os << "  " << to_string(lv);
        for (std::size_t pad = to_string(lv).size(); pad < 6; ++pad) os << ' ';
        os << uint_col(r.by_level[static_cast<std::size_t>(i)], 10) << "\n";
    }
    os << "  errors=" << r.errors << "  warnings=" << r.warnings << "\n";

    section(os, "response codes");
    if (r.status_codes.empty()) {
        os << "  (none)\n";
    } else {
        for (const auto& c : r.status_codes) {
            os << "  " << c.key << "  " << c.value << "\n";
        }
    }
    os << "  classes:";
    for (int cls = 1; cls <= 5; ++cls) {
        os << ' ' << cls << "xx=" << r.by_status_class[static_cast<std::size_t>(cls)];
    }
    os << "\n";

    section(os, "top services (by volume)");
    if (r.top_services.empty()) {
        os << "  (none)\n";
    } else {
        for (const auto& c : r.top_services) os << "  " << uint_col(c.value, 8) << "  " << c.key << "\n";
    }

    section(os, "top errors (level >= ERROR)");
    if (r.top_errors.empty()) {
        os << "  (none)\n";
    } else {
        for (const auto& c : r.top_errors) os << "  " << uint_col(c.value, 8) << "  " << c.key << "\n";
    }

    section(os, "failures by service (level >= ERROR or 5xx)");
    if (r.top_failure_services.empty()) {
        os << "  (none)\n";
    } else {
        for (const auto& c : r.top_failure_services) {
            os << "  " << uint_col(c.value, 8) << "  " << c.key << "\n";
        }
    }

    section(os, "busiest endpoints");
    if (r.busiest_endpoints.empty()) {
        os << "  (none)\n";
    } else {
        for (const auto& e : r.busiest_endpoints) {
            os << "  " << uint_col(e.count, 8) << "  " << e.endpoint;
            if (e.timed > 0) {
                os << "  mean=" << ms1(e.mean_ms) << "ms p50=" << lat(e.p50_ms)
                   << " p90=" << lat(e.p90_ms) << " p99=" << lat(e.p99_ms)
                   << " max=" << lat(e.max_ms);
            }
            os << "\n";
        }
    }

    section(os, "slowest endpoints (by mean latency)");
    if (r.slowest_endpoints.empty()) {
        os << "  (none)\n";
    } else {
        for (const auto& e : r.slowest_endpoints) {
            os << "  " << ms1(e.mean_ms) << "ms  " << e.endpoint << "  n=" << e.timed
               << " p90=" << lat(e.p90_ms) << " p99=" << lat(e.p99_ms) << "\n";
        }
    }

    section(os, ("traffic (" + format_interval(r.interval_ms) + " buckets)").c_str());
    if (r.timeline.empty()) {
        os << "  (none)\n";
    } else {
        for (const auto& t : r.timeline) {
            os << "  " << format_timestamp(t.bucket_start_ms) << "  req=" << t.requests
               << " err=" << t.errors << "\n";
        }
    }
}

} // namespace la
