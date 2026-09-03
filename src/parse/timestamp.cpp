#include "parse/timestamp.hpp"

#include <cstddef>

namespace la {
namespace {

bool all_digits(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// Read a fixed-width, all-digits unsigned field at [pos, pos+len).
bool take_uint(std::string_view s, std::size_t pos, std::size_t len, int& out) {
    if (pos + len > s.size()) return false;
    const std::string_view f = s.substr(pos, len);
    if (!all_digits(f)) return false;
    int v = 0;
    for (char c : f) v = v * 10 + (c - '0');
    out = v;
    return true;
}

constexpr bool is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

constexpr int days_in_month(int y, int m) {
    constexpr int t[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && is_leap(y)) return 29;
    return t[m - 1];
}

// Days from 1970-01-01 to the given civil date. Valid for any Gregorian date.
// (Howard Hinnant, "chrono-Compatible Low-Level Date Algorithms".)
constexpr std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * (m > 2 ? m - 3 : m + 9) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

} // namespace

std::optional<std::int64_t> parse_timestamp(std::string_view s) {
    // Shortest valid form is "YYYY-MM-DDThh:mm:ssZ" (20 chars).
    if (s.size() < 20) return std::nullopt;
    if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':') {
        return std::nullopt;
    }

    int year = 0, mon = 0, day = 0, hh = 0, mm = 0, ss = 0;
    if (!take_uint(s, 0, 4, year)) return std::nullopt;
    if (!take_uint(s, 5, 2, mon)) return std::nullopt;
    if (!take_uint(s, 8, 2, day)) return std::nullopt;
    if (!take_uint(s, 11, 2, hh)) return std::nullopt;
    if (!take_uint(s, 14, 2, mm)) return std::nullopt;
    if (!take_uint(s, 17, 2, ss)) return std::nullopt;

    if (mon < 1 || mon > 12) return std::nullopt;
    if (day < 1 || day > days_in_month(year, mon)) return std::nullopt;
    if (hh > 23 || mm > 59 || ss > 59) return std::nullopt;

    std::int64_t frac_ms = 0;
    std::size_t i = 19;
    if (s[i] == '.') {
        ++i;
        const std::size_t start = i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        if (i == start) return std::nullopt; // '.' with no digits

        const std::string_view digits = s.substr(start, i - start);
        std::int64_t scale = 100; // first fractional digit is hundreds of ms
        for (std::size_t k = 0; k < 3 && k < digits.size(); ++k) {
            frac_ms += (digits[k] - '0') * scale;
            scale /= 10;
        }
    }

    // Exactly a trailing 'Z' and nothing else.
    if (i != s.size() - 1 || s[i] != 'Z') return std::nullopt;

    const std::int64_t days = days_from_civil(
        year, static_cast<unsigned>(mon), static_cast<unsigned>(day));
    const std::int64_t secs = days * 86400 + hh * 3600 + mm * 60 + ss;
    return secs * 1000 + frac_ms;
}

} // namespace la
