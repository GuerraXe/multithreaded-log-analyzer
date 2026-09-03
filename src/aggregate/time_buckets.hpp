#pragma once

#include <cstdint>
#include <map>

namespace la {

struct BucketCounts {
    std::uint64_t requests = 0;
    std::uint64_t errors = 0;
};

// Traffic aggregated into fixed-width time windows. Keyed by the bucket start
// (epoch ms, floored to the interval), so iteration is chronological and the
// merged result is order-independent.
class TimeBuckets {
public:
    explicit TimeBuckets(std::int64_t interval_ms) : interval_ms_(interval_ms) {}

    void add(std::int64_t epoch_ms, bool is_error);
    void merge(const TimeBuckets& other);

    std::int64_t interval_ms() const { return interval_ms_; }
    const std::map<std::int64_t, BucketCounts>& buckets() const { return buckets_; }

    // Bucket start for a timestamp (floor division, correct for negatives).
    std::int64_t bucket_start(std::int64_t epoch_ms) const;

private:
    std::int64_t interval_ms_;
    std::map<std::int64_t, BucketCounts> buckets_;
};

} // namespace la
