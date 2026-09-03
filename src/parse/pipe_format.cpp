#include "parse/pipe_format.hpp"

#include "parse/timestamp.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace la {
namespace {

constexpr std::string_view kSep = " | ";

bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view s) {
    std::size_t b = 0, e = s.size();
    while (b < e && is_ws(s[b])) ++b;
    while (e > b && is_ws(s[e - 1])) --e;
    return s.substr(b, e - b);
}

bool all_digits(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

bool valid_service(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

// Parse the duration_ms field -- "<int>[.<frac>]" milliseconds, no sign, no
// exponent -- into integer microseconds, rounding the sub-microsecond
// remainder half-to-even. Returns false on malformed input or on an integer
// part wide enough to risk overflow.
bool parse_duration_us(std::string_view s, std::int64_t& out) {
    const std::size_t dot = s.find('.');
    const std::string_view ip = (dot == std::string_view::npos) ? s : s.substr(0, dot);
    const std::string_view fp = (dot == std::string_view::npos)
                                    ? std::string_view{}
                                    : s.substr(dot + 1);

    if (ip.empty() && fp.empty()) return false;
    if (!ip.empty() && !all_digits(ip)) return false;
    if (dot != std::string_view::npos && !all_digits(fp)) return false;
    if (ip.size() > 15) return false; // keep whole_ms * 1000 well inside int64

    std::int64_t whole_ms = 0;
    for (char c : ip) whole_ms = whole_ms * 10 + (c - '0');
    std::int64_t us = whole_ms * 1000;

    // First three fractional digits are microseconds (0.001 ms = 1 us);
    // the fourth decides half-to-even rounding.
    std::int64_t frac_us = 0;
    std::int64_t scale = 100;
    std::size_t k = 0;
    for (; k < 3 && k < fp.size(); ++k) {
        frac_us += (fp[k] - '0') * scale;
        scale /= 10;
    }
    if (k < fp.size()) {
        const int d = fp[k] - '0';
        bool rest_nonzero = false;
        for (std::size_t j = k + 1; j < fp.size(); ++j) {
            if (fp[j] != '0') {
                rest_nonzero = true;
                break;
            }
        }
        if (d > 5 || (d == 5 && (rest_nonzero || (frac_us & 1) != 0))) ++frac_us;
    }

    out = us + frac_us;
    return true;
}

} // namespace

ParseResult PipeDelimitedFormat::parse_line(std::string_view line) const {
    // Tolerate CRLF input; every field is trimmed below regardless.
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

    // Six leading fields; the message is everything after the sixth separator
    // and may itself contain further " | " sequences (documented limitation).
    std::array<std::string_view, 6> f{};
    std::string_view rest = line;
    for (int i = 0; i < 6; ++i) {
        const std::size_t p = rest.find(kSep);
        if (p == std::string_view::npos) {
            return ParseResult::failure(ParseError::FieldCount);
        }
        f[static_cast<std::size_t>(i)] = rest.substr(0, p);
        rest.remove_prefix(p + kSep.size());
    }
    const std::string_view msg_field = rest;

    LogRecord rec;

    if (const auto ts = parse_timestamp(trim(f[0]))) {
        rec.epoch_ms = *ts;
    } else {
        return ParseResult::failure(ParseError::Timestamp);
    }

    if (const auto lv = parse_level(trim(f[1]))) {
        rec.level = *lv;
    } else {
        return ParseResult::failure(ParseError::Level);
    }

    const std::string_view svc = trim(f[2]);
    if (!valid_service(svc)) return ParseResult::failure(ParseError::Service);
    rec.service = svc;

    const std::string_view req = trim(f[3]);
    if (req.empty()) {
        rec.method = Method::None;
        rec.path = {};
    } else {
        const std::size_t sp = req.find(' ');
        if (sp == std::string_view::npos) {
            return ParseResult::failure(ParseError::Request);
        }
        const auto pm = parse_method(req.substr(0, sp));
        const std::string_view path = req.substr(sp + 1);
        if (!pm || path.empty() || path.front() != '/') {
            return ParseResult::failure(ParseError::Request);
        }
        rec.method = *pm;
        rec.path = path;
    }

    const std::string_view st = trim(f[4]);
    if (st.empty()) {
        rec.status = 0;
    } else {
        if (!all_digits(st) || st.size() > 3) {
            return ParseResult::failure(ParseError::Status);
        }
        int v = 0;
        for (char c : st) v = v * 10 + (c - '0');
        if (v < 100 || v > 599) return ParseResult::failure(ParseError::Status);
        rec.status = static_cast<std::uint16_t>(v);
    }

    const std::string_view du = trim(f[5]);
    if (du.empty()) {
        rec.duration_us = -1;
    } else {
        std::int64_t us = 0;
        if (!parse_duration_us(du, us)) {
            return ParseResult::failure(ParseError::Duration);
        }
        rec.duration_us = us;
    }

    rec.message = trim(msg_field);

    return ParseResult::success(rec);
}

} // namespace la
