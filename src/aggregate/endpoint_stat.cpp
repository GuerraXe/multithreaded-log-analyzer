#include "aggregate/endpoint_stat.hpp"

#include <cmath>

namespace la {

void EndpointStat::observe(std::int64_t duration_us) {
    ++count;
    if (duration_us < 0) return;

    if (timed == 0 || duration_us < min_us) min_us = duration_us;
    if (timed == 0 || duration_us > max_us) max_us = duration_us;
    ++timed;
    sum_us += duration_us;
    sum_sq_us += static_cast<double>(duration_us) * static_cast<double>(duration_us);
    hist.add(duration_us);
}

void EndpointStat::merge(const EndpointStat& other) {
    if (other.timed > 0) {
        if (timed == 0 || other.min_us < min_us) min_us = other.min_us;
        if (timed == 0 || other.max_us > max_us) max_us = other.max_us;
    }
    count += other.count;
    timed += other.timed;
    sum_us += other.sum_us;
    sum_sq_us += other.sum_sq_us;
    hist.merge(other.hist);
}

double EndpointStat::mean_us() const {
    if (timed == 0) return 0.0;
    return static_cast<double>(sum_us) / static_cast<double>(timed);
}

double EndpointStat::stddev_us() const {
    if (timed < 2) return 0.0;
    const double n = static_cast<double>(timed);
    const double mean = static_cast<double>(sum_us) / n;
    const double variance = sum_sq_us / n - mean * mean;
    return variance > 0.0 ? std::sqrt(variance) : 0.0;
}

} // namespace la
