#include "bench/benchmark.hpp"

#include "concurrency/parallel_aggregate.hpp"
#include "core/process_info.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ostream>
#include <set>
#include <thread>

namespace la {
namespace {

double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

std::vector<unsigned> default_thread_counts() {
    std::set<unsigned> s = {1u, 2u, 4u, 8u};
    const unsigned hw = std::thread::hardware_concurrency();
    if (hw > 0) s.insert(hw);
    return {s.begin(), s.end()};
}

// Minimal JSON string-body escaper (no surrounding quotes). The verdict is the
// only free-form string this report emits; keep its output valid even if the
// wording ever grows a quote, backslash, or control character.
std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

std::string format_verdict(const std::vector<BenchRow>& rows) {
    const BenchRow* best = nullptr;
    for (const auto& r : rows) {
        if (r.threads == 1) continue;
        if (best == nullptr || r.speedup > best->speedup) best = &r;
    }
    if (best == nullptr) return "single configuration only; nothing to compare";

    char buf[160];
    if (best->speedup >= 1.10) {
        std::snprintf(buf, sizeof buf,
                      "multithreading helped: %.2fx at %u threads (%.0f%% efficiency)",
                      best->speedup, best->threads, 100.0 * best->efficiency);
    } else if (best->speedup <= 0.95) {
        std::snprintf(buf, sizeof buf,
                      "multithreading did not help: best %.2fx at %u threads - overhead dominates",
                      best->speedup, best->threads);
    } else {
        std::snprintf(buf, sizeof buf,
                      "multithreading was roughly neutral: best %.2fx at %u threads",
                      best->speedup, best->threads);
    }
    return buf;
}

} // namespace

BenchmarkReport run_benchmark_core(std::string_view buffer, const ILogFormat& fmt,
                                   const RecordFilter& filter, const BenchmarkOptions& opt) {
    BenchmarkReport report;
    report.repeat = opt.repeat;
    report.warmup = opt.warmup;
    report.bytes = buffer.size();

    std::vector<unsigned> tcs = opt.thread_counts;
    if (tcs.empty()) tcs = default_thread_counts();
    std::sort(tcs.begin(), tcs.end());
    tcs.erase(std::unique(tcs.begin(), tcs.end()), tcs.end());
    if (tcs.empty()) tcs.push_back(1u);
    report.baseline_threads = tcs.front();

    const int repeat = opt.repeat < 1 ? 1 : opt.repeat;
    const int warmup = opt.warmup < 0 ? 0 : opt.warmup;

    double baseline_median = 0.0;
    for (const unsigned t : tcs) {
        for (int w = 0; w < warmup; ++w) {
            volatile auto sink = parallel_aggregate(buffer, fmt, filter, opt.aggregate, t);
            (void)sink;
        }

        std::vector<double> samples;
        samples.reserve(static_cast<std::size_t>(repeat));
        Aggregate last(opt.aggregate.interval_ms);
        for (int r = 0; r < repeat; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            last = parallel_aggregate(buffer, fmt, filter, opt.aggregate, t);
            const auto t1 = std::chrono::steady_clock::now();
            samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        if (report.records == 0 && report.kept == 0 && report.malformed == 0) {
            report.records = last.records;
            report.kept = last.kept;
            report.malformed = last.malformed;
        }

        BenchRow row;
        row.threads = t;
        row.median_ms = median(samples);
        row.min_ms = *std::min_element(samples.begin(), samples.end());
        row.max_ms = *std::max_element(samples.begin(), samples.end());
        const double secs = row.median_ms / 1000.0;
        if (secs > 0.0) {
            row.records_per_s = static_cast<double>(last.records) / secs;
            row.mb_per_s = (static_cast<double>(buffer.size()) / 1'000'000.0) / secs;
        }
        if (t == report.baseline_threads) baseline_median = row.median_ms;
        row.speedup = (row.median_ms > 0.0) ? baseline_median / row.median_ms : 0.0;
        row.efficiency = (t > 0) ? row.speedup / static_cast<double>(t) : 0.0;
        report.rows.push_back(row);
    }

    report.peak_working_set_bytes = peak_working_set_bytes();
    report.verdict = format_verdict(report.rows);
    return report;
}

void render_benchmark_text(const BenchmarkReport& r, std::ostream& os) {
    char buf[256];
    os << "benchmark: " << r.records << " records, " << r.malformed << " malformed, "
       << r.bytes << " bytes\n";
    os << "protocol: " << r.warmup << " warmup + " << r.repeat
       << " measured runs per thread count; median of the parse+aggregate phase\n\n";

    os << "  threads   median_ms     min     max     records/s      MB/s   speedup   eff\n";
    for (const auto& row : r.rows) {
        std::snprintf(buf, sizeof buf,
                      "  %7u  %10.2f  %6.2f  %6.2f  %12.0f  %8.1f   %6.2fx  %4.0f%%\n",
                      row.threads, row.median_ms, row.min_ms, row.max_ms,
                      row.records_per_s, row.mb_per_s, row.speedup, 100.0 * row.efficiency);
        os << buf;
    }

    os << "\nverdict: " << r.verdict << "\n";
    if (r.peak_working_set_bytes) {
        std::snprintf(buf, sizeof buf, "peak working set: %.1f MB\n",
                      static_cast<double>(*r.peak_working_set_bytes) / 1'048'576.0);
        os << buf;
    }
}

void render_benchmark_json(const BenchmarkReport& r, std::ostream& os) {
    char buf[256];
    os << "{\n";
    os << "  \"records\": " << r.records << ",\n";
    os << "  \"kept\": " << r.kept << ",\n";
    os << "  \"malformed\": " << r.malformed << ",\n";
    os << "  \"bytes\": " << r.bytes << ",\n";
    os << "  \"warmup\": " << r.warmup << ",\n";
    os << "  \"repeat\": " << r.repeat << ",\n";
    os << "  \"baseline_threads\": " << r.baseline_threads << ",\n";
    if (r.peak_working_set_bytes) {
        os << "  \"peak_working_set_bytes\": " << *r.peak_working_set_bytes << ",\n";
    } else {
        os << "  \"peak_working_set_bytes\": null,\n";
    }
    os << "  \"rows\": [\n";
    for (std::size_t i = 0; i < r.rows.size(); ++i) {
        const auto& row = r.rows[i];
        std::snprintf(buf, sizeof buf,
                      "    { \"threads\": %u, \"median_ms\": %.4f, \"min_ms\": %.4f, "
                      "\"max_ms\": %.4f, \"records_per_s\": %.2f, \"mb_per_s\": %.4f, "
                      "\"speedup\": %.4f, \"efficiency\": %.4f }%s\n",
                      row.threads, row.median_ms, row.min_ms, row.max_ms, row.records_per_s,
                      row.mb_per_s, row.speedup, row.efficiency,
                      (i + 1 < r.rows.size()) ? "," : "");
        os << buf;
    }
    os << "  ],\n";
    os << "  \"verdict\": \"" << json_escape(r.verdict) << "\"\n";
    os << "}\n";
}

} // namespace la
