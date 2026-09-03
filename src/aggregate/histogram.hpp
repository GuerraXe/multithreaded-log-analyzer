#pragma once

#include <array>
#include <cstdint>

namespace la {

// Fixed-bucket latency histogram. Bucket boundaries are compile-time
// constants, so two histograms are always mergeable and a percentile query is
// a pure function of the bin counts -- this is what makes multithreaded
// latency percentiles bit-identical to the sequential result (SPEC CR-1).
//
// Bucket i counts samples with duration in (edge[i-1], edge[i]] microseconds;
// bucket 0 is (-inf, 1000us]; the final bucket is (10s, +inf) and its edge is
// reported as INT64_MAX ("overflow").
class LatencyHistogram {
public:
    static constexpr int kBucketCount = 14;

    // Upper edges in microseconds. Index kBucketCount-1 is the +inf sentinel.
    static constexpr std::array<std::int64_t, kBucketCount> kEdgesUs = {
        1'000,      2'000,      5'000,       10'000,       20'000,
        50'000,     100'000,    200'000,     500'000,      1'000'000,
        2'000'000,  5'000'000,  10'000'000,  INT64_MAX,
    };

    void add(std::int64_t duration_us);
    void merge(const LatencyHistogram& other);

    std::uint64_t total() const;
    const std::array<std::uint64_t, kBucketCount>& bins() const { return bins_; }

    // Upper edge (microseconds) of the bucket holding the p-th percentile
    // sample, p in (0, 100]. Returns -1 when empty; INT64_MAX when the
    // percentile falls in the overflow bucket.
    std::int64_t percentile_us(double p) const;

private:
    std::array<std::uint64_t, kBucketCount> bins_{};
};

} // namespace la
