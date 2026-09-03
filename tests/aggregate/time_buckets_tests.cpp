#include "support/test_framework.hpp"

#include "aggregate/time_buckets.hpp"

#include <cstdint>

using namespace la;

TEST_CASE("time_buckets: keys are floored to the interval") {
    TimeBuckets tb(60'000); // 1 minute
    tb.add(0, false);
    tb.add(59'999, false);
    tb.add(60'000, true);
    tb.add(125'000, false);

    const auto& b = tb.buckets();
    CHECK_EQ(b.size(), std::size_t{3});
    CHECK_EQ(b.at(0).requests, std::uint64_t{2});
    CHECK_EQ(b.at(0).errors, std::uint64_t{0});
    CHECK_EQ(b.at(60'000).requests, std::uint64_t{1});
    CHECK_EQ(b.at(60'000).errors, std::uint64_t{1});
    CHECK_EQ(b.at(120'000).requests, std::uint64_t{1});
}

TEST_CASE("time_buckets: floor division is correct for negative timestamps") {
    TimeBuckets tb(1'000);
    CHECK_EQ(tb.bucket_start(-1), std::int64_t{-1'000});
    CHECK_EQ(tb.bucket_start(-1'000), std::int64_t{-1'000});
    CHECK_EQ(tb.bucket_start(-1'001), std::int64_t{-2'000});
    CHECK_EQ(tb.bucket_start(999), std::int64_t{0});
}

TEST_CASE("time_buckets: merge is a per-key sum") {
    TimeBuckets a(60'000);
    TimeBuckets b(60'000);
    a.add(0, true);
    a.add(60'000, false);
    b.add(0, false);
    b.add(120'000, true);
    a.merge(b);

    const auto& m = a.buckets();
    CHECK_EQ(m.at(0).requests, std::uint64_t{2});
    CHECK_EQ(m.at(0).errors, std::uint64_t{1});
    CHECK_EQ(m.at(60'000).requests, std::uint64_t{1});
    CHECK_EQ(m.at(120'000).requests, std::uint64_t{1});
    CHECK_EQ(m.at(120'000).errors, std::uint64_t{1});
}
