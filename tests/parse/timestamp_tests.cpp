#include "support/test_framework.hpp"

#include "parse/timestamp.hpp"

#include <cstdint>

using la::parse_timestamp;

namespace {
std::int64_t ms(std::string_view s) {
    const auto v = parse_timestamp(s);
    CHECK(v.has_value());
    return *v;
}
} // namespace

TEST_CASE("timestamp: epoch anchors") {
    CHECK_EQ(ms("1970-01-01T00:00:00Z"), std::int64_t{0});
    CHECK_EQ(ms("1970-01-01T00:00:01Z"), std::int64_t{1000});
    CHECK_EQ(ms("1970-01-02T00:00:00Z"), std::int64_t{86'400'000});
    CHECK_EQ(ms("2000-01-01T00:00:00Z"), std::int64_t{946'684'800'000});
}

TEST_CASE("timestamp: pre-epoch is negative") {
    CHECK_EQ(ms("1969-12-31T23:59:59Z"), std::int64_t{-1000});
}

TEST_CASE("timestamp: fractional seconds truncate to milliseconds") {
    CHECK_EQ(ms("2000-01-01T00:00:00.500Z"), std::int64_t{946'684'800'500});
    CHECK_EQ(ms("2000-01-01T00:00:00.7Z"), std::int64_t{946'684'800'700});
    CHECK_EQ(ms("2000-01-01T00:00:00.7899Z"), std::int64_t{946'684'800'789});
    CHECK_EQ(ms("2000-01-01T00:00:00.000000001Z"), std::int64_t{946'684'800'000});
}

TEST_CASE("timestamp: leap-year day is valid, non-leap is not") {
    CHECK(parse_timestamp("2024-02-29T00:00:00Z").has_value());
    CHECK(!parse_timestamp("2023-02-29T00:00:00Z").has_value());
    CHECK(!parse_timestamp("1900-02-29T00:00:00Z").has_value()); // century, not leap
    CHECK(parse_timestamp("2000-02-29T00:00:00Z").has_value());  // 400-year, leap
}

TEST_CASE("timestamp: out-of-range calendar fields are rejected") {
    CHECK(!parse_timestamp("2023-00-01T00:00:00Z").has_value());
    CHECK(!parse_timestamp("2023-13-01T00:00:00Z").has_value());
    CHECK(!parse_timestamp("2023-01-00T00:00:00Z").has_value());
    CHECK(!parse_timestamp("2023-01-32T00:00:00Z").has_value());
    CHECK(!parse_timestamp("2023-04-31T00:00:00Z").has_value());
}

TEST_CASE("timestamp: out-of-range clock fields are rejected") {
    CHECK(!parse_timestamp("2023-01-01T24:00:00Z").has_value());
    CHECK(!parse_timestamp("2023-01-01T00:60:00Z").has_value());
    CHECK(!parse_timestamp("2023-01-01T00:00:60Z").has_value());
}

TEST_CASE("timestamp: grammar violations are rejected") {
    CHECK(!parse_timestamp("").has_value());
    CHECK(!parse_timestamp("garbage").has_value());
    CHECK(!parse_timestamp("2023-01-01 00:00:00Z").has_value());  // space, not 'T'
    CHECK(!parse_timestamp("2023-01-01t00:00:00Z").has_value());  // lower-case 't'
    CHECK(!parse_timestamp("2023-01-01T00:00:00").has_value());   // missing 'Z'
    CHECK(!parse_timestamp("2023-01-01T00:00:00z").has_value());  // lower-case 'z'
    CHECK(!parse_timestamp("2023-01-01T00:00:00.Z").has_value()); // '.' with no digits
    CHECK(!parse_timestamp("2023-01-01T00:00:00Z ").has_value()); // trailing space
    CHECK(!parse_timestamp("2023-1-01T00:00:00Z").has_value());   // one-digit month
    CHECK(!parse_timestamp("20xx-01-01T00:00:00Z").has_value());  // non-digit year
}
