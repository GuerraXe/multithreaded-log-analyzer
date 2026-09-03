#include "aggregate/histogram.hpp"

#include <cmath>

namespace la {

void LatencyHistogram::add(std::int64_t duration_us) {
    for (int i = 0; i < kBucketCount; ++i) {
        if (duration_us <= kEdgesUs[i]) {
            ++bins_[static_cast<std::size_t>(i)];
            return;
        }
    }
    ++bins_[kBucketCount - 1]; // unreachable: last edge is INT64_MAX
}

void LatencyHistogram::merge(const LatencyHistogram& other) {
    for (int i = 0; i < kBucketCount; ++i) {
        bins_[static_cast<std::size_t>(i)] += other.bins_[static_cast<std::size_t>(i)];
    }
}

std::uint64_t LatencyHistogram::total() const {
    std::uint64_t n = 0;
    for (const std::uint64_t b : bins_) n += b;
    return n;
}

std::int64_t LatencyHistogram::percentile_us(double p) const {
    const std::uint64_t n = total();
    if (n == 0) return -1;

    const long double target = (static_cast<long double>(p) / 100.0L) *
                               static_cast<long double>(n);
    std::uint64_t need = static_cast<std::uint64_t>(std::ceil(static_cast<double>(target)));
    if (need == 0) need = 1;
    if (need > n) need = n;

    std::uint64_t cum = 0;
    for (int i = 0; i < kBucketCount; ++i) {
        cum += bins_[static_cast<std::size_t>(i)];
        if (cum >= need) return kEdgesUs[i];
    }
    return kEdgesUs[kBucketCount - 1];
}

} // namespace la
