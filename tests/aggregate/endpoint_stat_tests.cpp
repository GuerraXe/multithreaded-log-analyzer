#include "support/test_framework.hpp"

#include "aggregate/endpoint_stat.hpp"

#include <cmath>
#include <cstdint>

using namespace la;

namespace {
bool close(double a, double b, double rel = 1e-6) {
    const double scale = std::max({1.0, std::fabs(a), std::fabs(b)});
    return std::fabs(a - b) <= rel * scale;
}
} // namespace

TEST_CASE("endpoint_stat: counts, extremes and exact sum") {
    EndpointStat s;
    s.observe(10'000); // 10 ms
    s.observe(30'000);
    s.observe(20'000);
    s.observe(-1); // request with no duration
    CHECK_EQ(s.count, std::uint64_t{4});
    CHECK_EQ(s.timed, std::uint64_t{3});
    CHECK_EQ(s.min_us, std::int64_t{10'000});
    CHECK_EQ(s.max_us, std::int64_t{30'000});
    CHECK_EQ(s.sum_us, std::int64_t{60'000});
    CHECK(close(s.mean_us(), 20'000.0));
}

TEST_CASE("endpoint_stat: population standard deviation") {
    EndpointStat s;
    s.observe(10'000);
    s.observe(20'000);
    s.observe(30'000);
    // population stddev of {10k,20k,30k} = sqrt(200e6/3) ~= 8164.9658
    CHECK(close(s.stddev_us(), 8164.9658092773, 1e-6));
}

TEST_CASE("endpoint_stat: stddev is zero below two samples") {
    EndpointStat s;
    CHECK(close(s.stddev_us(), 0.0));
    s.observe(5'000);
    CHECK(close(s.stddev_us(), 0.0));
}

TEST_CASE("endpoint_stat: merge combines, and merging empty is identity") {
    EndpointStat a;
    a.observe(10'000);
    a.observe(40'000);

    EndpointStat b;
    b.observe(5'000);
    b.observe(50'000);

    EndpointStat merged = a;
    merged.merge(b);
    CHECK_EQ(merged.count, std::uint64_t{4});
    CHECK_EQ(merged.timed, std::uint64_t{4});
    CHECK_EQ(merged.min_us, std::int64_t{5'000});
    CHECK_EQ(merged.max_us, std::int64_t{50'000});
    CHECK_EQ(merged.sum_us, std::int64_t{105'000});

    EndpointStat identity = a;
    identity.merge(EndpointStat{});
    CHECK_EQ(identity.count, a.count);
    CHECK_EQ(identity.sum_us, a.sum_us);
    CHECK_EQ(identity.min_us, a.min_us);
    CHECK_EQ(identity.max_us, a.max_us);
}
