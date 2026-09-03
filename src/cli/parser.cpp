#include "cli/parser.hpp"

#include "parse/log_record.hpp"
#include "parse/timestamp.hpp"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace la {
namespace {

bool to_int(std::string_view s, int& out) {
    if (s.empty()) return false;
    int v = 0;
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || p != s.data() + s.size()) return false;
    out = v;
    return true;
}

bool to_u64(std::string_view s, std::uint64_t& out) {
    if (s.empty()) return false;
    std::uint64_t v = 0;
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || p != s.data() + s.size()) return false;
    out = v;
    return true;
}

std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t p = s.find(sep, start);
        if (p == std::string_view::npos) {
            out.push_back(s.substr(start));
            return out;
        }
        out.push_back(s.substr(start, p - start));
        start = p + 1;
    }
}

// "30s" / "5m" / "2h" -> milliseconds; a bare integer is seconds. Value must
// be a positive integer.
bool parse_interval_ms(std::string_view s, std::int64_t& out) {
    if (s.empty()) return false;
    const char unit = s.back();
    std::int64_t mult = 1000;
    std::string_view num = s;
    if (unit == 's') {
        mult = 1000;
        num = s.substr(0, s.size() - 1);
    } else if (unit == 'm') {
        mult = 60'000;
        num = s.substr(0, s.size() - 1);
    } else if (unit == 'h') {
        mult = 3'600'000;
        num = s.substr(0, s.size() - 1);
    } else if (unit < '0' || unit > '9') {
        return false;
    }
    std::uint64_t v = 0;
    if (!to_u64(num, v) || v == 0) return false;
    out = static_cast<std::int64_t>(v) * mult;
    return true;
}

// "2", "2xx", "2XX" -> 2. Digit must be 1..5.
bool parse_status_class(std::string_view s, int& out) {
    if (s.empty() || s[0] < '1' || s[0] > '5') return false;
    if (s.size() == 1) {
        out = s[0] - '0';
        return true;
    }
    if (s.size() == 3 && (s[1] == 'x' || s[1] == 'X') && (s[2] == 'x' || s[2] == 'X')) {
        out = s[0] - '0';
        return true;
    }
    return false;
}

} // namespace

ArgParse parse_args(int argc, char** argv) {
    ArgParse r;
    if (argc < 2) {
        r.error = "no command given; try 'loganalyzer help'";
        return r;
    }

    const std::string_view cmd = argv[1];
    Options& o = r.options;

    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        o.command = Command::Version;
        r.ok = true;
        return r;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        o.command = Command::Help;
        if (argc >= 3) o.help_topic = argv[2];
        r.ok = true;
        return r;
    }
    if (cmd == "analyze") {
        o.command = Command::Analyze;
    } else if (cmd == "benchmark") {
        o.command = Command::Benchmark;
    } else if (cmd == "gen") {
        o.command = Command::Gen;
    } else {
        r.error = "unknown command '" + std::string(cmd) + "'";
        return r;
    }

    int i = 2;
    auto need = [&](std::string_view name) -> std::optional<std::string_view> {
        if (i + 1 >= argc) {
            r.error = "option " + std::string(name) + " requires a value";
            return std::nullopt;
        }
        return std::string_view(argv[++i]);
    };

    std::vector<std::string_view> positionals;

    for (; i < argc; ++i) {
        const std::string_view a = argv[i];
        const bool is_flag = a.size() > 1 && a[0] == '-';
        if (!is_flag) {
            positionals.push_back(a);
            continue;
        }

        if (a == "--threads") {
            const auto v = need(a);
            if (!v) return r;
            int t = 0;
            if (!to_int(*v, t) || t < 0) {
                r.error = "invalid --threads value '" + std::string(*v) + "'";
                return r;
            }
            o.threads = t;
        } else if (a == "--format") {
            const auto v = need(a);
            if (!v) return r;
            o.format = std::string(*v);
        } else if (a == "--from") {
            const auto v = need(a);
            if (!v) return r;
            const auto ts = parse_timestamp(*v);
            if (!ts) {
                r.error = "invalid --from timestamp '" + std::string(*v) + "'";
                return r;
            }
            o.filter.from_ms = *ts;
        } else if (a == "--to") {
            const auto v = need(a);
            if (!v) return r;
            const auto ts = parse_timestamp(*v);
            if (!ts) {
                r.error = "invalid --to timestamp '" + std::string(*v) + "'";
                return r;
            }
            o.filter.to_ms = *ts;
        } else if (a == "--level") {
            const auto v = need(a);
            if (!v) return r;
            const auto lv = parse_level(*v);
            if (!lv) {
                r.error = "invalid --level '" + std::string(*v) + "'";
                return r;
            }
            o.filter.min_level = *lv;
        } else if (a == "--level-only") {
            const auto v = need(a);
            if (!v) return r;
            for (const auto part : split(*v, ',')) {
                const auto lv = parse_level(part);
                if (!lv) {
                    r.error = "invalid level in --level-only: '" + std::string(part) + "'";
                    return r;
                }
                o.filter.level_only.push_back(*lv);
            }
        } else if (a == "--service") {
            const auto v = need(a);
            if (!v) return r;
            if (v->empty()) {
                r.error = "--service value must not be empty";
                return r;
            }
            o.filter.services.emplace_back(*v);
        } else if (a == "--status-class") {
            const auto v = need(a);
            if (!v) return r;
            int c = 0;
            if (!parse_status_class(*v, c)) {
                r.error = "invalid --status-class '" + std::string(*v) + "' (expected 1xx..5xx)";
                return r;
            }
            o.filter.status_classes.push_back(c);
        } else if (a == "--path-prefix") {
            const auto v = need(a);
            if (!v) return r;
            if (v->empty()) {
                r.error = "--path-prefix value must not be empty";
                return r;
            }
            o.filter.path_prefix = std::string(*v);
        } else if (a == "--path-contains") {
            const auto v = need(a);
            if (!v) return r;
            if (v->empty()) {
                r.error = "--path-contains value must not be empty";
                return r;
            }
            o.filter.path_contains = std::string(*v);
        } else if (a == "--top") {
            const auto v = need(a);
            if (!v) return r;
            int n = 0;
            if (!to_int(*v, n) || n <= 0) {
                r.error = "invalid --top value '" + std::string(*v) + "'";
                return r;
            }
            o.top_n = n;
        } else if (a == "--interval") {
            const auto v = need(a);
            if (!v) return r;
            std::int64_t ms = 0;
            if (!parse_interval_ms(*v, ms)) {
                r.error = "invalid --interval '" + std::string(*v) + "' (expected e.g. 30s, 1m, 1h)";
                return r;
            }
            o.interval_ms = ms;
        } else if (a == "--exact-percentiles") {
            o.exact_percentiles = true;
        } else if (a == "--report") {
            const auto v = need(a);
            if (!v) return r;
            if (*v == "text") {
                o.report = ReportFormat::Text;
            } else if (*v == "json") {
                o.report = ReportFormat::Json;
            } else {
                r.error = "invalid --report '" + std::string(*v) + "' (expected text|json)";
                return r;
            }
        } else if (a == "--strict") {
            o.strict = true;
        } else if (a == "--show-malformed") {
            const auto v = need(a);
            if (!v) return r;
            int n = 0;
            if (!to_int(*v, n) || n < 0) {
                r.error = "invalid --show-malformed value '" + std::string(*v) + "'";
                return r;
            }
            o.show_malformed = n;
        } else if (a == "-o" || a == "--output") {
            const auto v = need(a);
            if (!v) return r;
            o.output_path = std::string(*v);
        } else if (a == "--quiet") {
            o.quiet = true;
        } else if (a == "--verbose") {
            o.quiet = false;
        } else if (a == "--no-color") {
            o.no_color = true;
        } else if (a == "--threads-list") {
            const auto v = need(a);
            if (!v) return r;
            o.threads_list.clear();
            for (const auto part : split(*v, ',')) {
                int t = 0;
                if (!to_int(part, t) || t < 0) {
                    r.error = "invalid value in --threads-list: '" + std::string(part) + "'";
                    return r;
                }
                o.threads_list.push_back(t);
            }
        } else if (a == "--repeat") {
            const auto v = need(a);
            if (!v) return r;
            int n = 0;
            if (!to_int(*v, n) || n <= 0) {
                r.error = "invalid --repeat value '" + std::string(*v) + "'";
                return r;
            }
            o.repeat = n;
        } else if (a == "--warmup") {
            const auto v = need(a);
            if (!v) return r;
            int n = 0;
            if (!to_int(*v, n) || n < 0) {
                r.error = "invalid --warmup value '" + std::string(*v) + "'";
                return r;
            }
            o.warmup = n;
        } else if (a == "--lines") {
            const auto v = need(a);
            if (!v) return r;
            std::uint64_t n = 0;
            if (!to_u64(*v, n) || n == 0) {
                r.error = "invalid --lines value '" + std::string(*v) + "'";
                return r;
            }
            o.gen_lines = n;
        } else if (a == "--seed") {
            const auto v = need(a);
            if (!v) return r;
            std::uint64_t n = 0;
            if (!to_u64(*v, n)) {
                r.error = "invalid --seed value '" + std::string(*v) + "'";
                return r;
            }
            o.gen_seed = n;
        } else {
            r.error = "unknown option '" + std::string(a) + "'";
            return r;
        }
    }

    if (positionals.empty()) {
        r.error = std::string(cmd) + " requires a <file> argument";
        return r;
    }
    if (positionals.size() > 1) {
        r.error = "unexpected extra argument '" + std::string(positionals[1]) + "'";
        return r;
    }
    o.input_path = std::string(positionals[0]);

    // --level-only takes precedence over --level.
    if (!o.filter.level_only.empty()) o.filter.min_level.reset();

    r.ok = true;
    return r;
}

} // namespace la
