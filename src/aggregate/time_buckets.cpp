#include "aggregate/time_buckets.hpp"

namespace la {
namespace {

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    const std::int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

} // namespace

std::int64_t TimeBuckets::bucket_start(std::int64_t epoch_ms) const {
    // Guard against a non-positive interval (the CLI rejects one, but a direct
    // caller could pass it) so this never divides by zero.
    const std::int64_t iv = interval_ms_ > 0 ? interval_ms_ : 1;
    return floor_div(epoch_ms, iv) * iv;
}

void TimeBuckets::add(std::int64_t epoch_ms, bool is_error) {
    BucketCounts& b = buckets_[bucket_start(epoch_ms)];
    ++b.requests;
    if (is_error) ++b.errors;
}

void TimeBuckets::merge(const TimeBuckets& other) {
    for (const auto& [start, counts] : other.buckets_) {
        BucketCounts& b = buckets_[start];
        b.requests += counts.requests;
        b.errors += counts.errors;
    }
}

} // namespace la
