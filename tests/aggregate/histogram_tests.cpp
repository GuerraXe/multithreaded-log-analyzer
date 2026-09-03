#include "support/test_framework.hpp"

#include "aggregate/histogram.hpp"

#include <cstdint>

using namespace la;

TEST_CASE("histogram: bucket edges are inclusive on the upper bound") {
    LatencyHistogram h;
    h.add(1'000);  // == 1ms edge -> bucket 0
    h.add(1'001);  // just over -> bucket 1
    h.add(0);      // -> bucket 0
    const auto& b = h.bins();
    CHECK_EQ(b[0], std::uint64_t{2});
    CHECK_EQ(b[1], std::uint64_t{1});
    CHECK_EQ(h.total(), std::uint64_t{3});
}

TEST_CASE("histogram: very large samples land in the overflow bucket") {
    LatencyHistogram h;
    h.add(999'000'000); // 999 s
    CHECK_EQ(h.bins()[LatencyHistogram::kBucketCount - 1], std::uint64_t{1});
    CHECK_EQ(h.percentile_us(99.0), INT64_MAX);
}

TEST_CASE("histogram: merge adds bin counts") {
    LatencyHistogram a;
    LatencyHistogram b;
    a.add(1'500); // bucket 1
    b.add(1'700); // bucket 1
    b.add(30'000); // bucket 5 (<=50ms, >20ms)
    a.merge(b);
    CHECK_EQ(a.bins()[1], std::uint64_t{2});
    CHECK_EQ(a.bins()[5], std::uint64_t{1});
    CHECK_EQ(a.total(), std::uint64_t{3});
}

TEST_CASE("histogram: percentiles from a known fill") {
    LatencyHistogram h;
    for (int i = 0; i < 90; ++i) h.add(1'000);   // bucket 0  (<=1ms)
    for (int i = 0; i < 9; ++i) h.add(1'500);    // bucket 1  (<=2ms)
    h.add(7'000);                                 // bucket 3  (<=10ms)
    CHECK_EQ(h.total(), std::uint64_t{100});
    CHECK_EQ(h.percentile_us(50.0), std::int64_t{1'000});  // within first 90
    CHECK_EQ(h.percentile_us(90.0), std::int64_t{1'000});  // 90th item still bucket 0
    CHECK_EQ(h.percentile_us(99.0), std::int64_t{2'000});  // 99th item in bucket 1
    CHECK_EQ(h.percentile_us(100.0), std::int64_t{10'000}); // last item in bucket 3
}

TEST_CASE("histogram: empty percentile is -1") {
    LatencyHistogram h;
    CHECK_EQ(h.percentile_us(50.0), std::int64_t{-1});
}
