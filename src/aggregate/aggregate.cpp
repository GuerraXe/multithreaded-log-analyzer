#include "aggregate/aggregate.hpp"

#include <string>

namespace la {

bool is_failure(const LogRecord& r) {
    if (level_index(r.level) >= level_index(Level::Error)) return true;
    if (r.status >= 500 && r.status <= 599) return true;
    return false;
}

void Aggregate::observe(const LogRecord& r) {
    ++kept;
    ++by_level[static_cast<std::size_t>(level_index(r.level))];

    if (r.status != 0) {
        ++by_status[r.status];
        const int cls = r.status / 100;
        if (cls >= 1 && cls <= 5) ++by_status_class[static_cast<std::size_t>(cls)];
    }

    ++by_service[std::string(r.service)];

    if (level_index(r.level) >= level_index(Level::Error)) {
        ++error_messages[std::string(r.message)];
    }

    if (r.method != Method::None) {
        std::string key(to_string(r.method));
        key += ' ';
        key.append(r.path);
        endpoints[key].observe(r.duration_us);
    }

    const bool failure = is_failure(r);
    if (failure) ++failures_by_service[std::string(r.service)];
    time_buckets.add(r.epoch_ms, failure);
}

void Aggregate::merge(const Aggregate& other) {
    total_lines += other.total_lines;
    records += other.records;
    kept += other.kept;
    malformed += other.malformed;
    blank += other.blank;
    bytes += other.bytes;

    for (std::size_t i = 0; i < kLevelCount; ++i) by_level[i] += other.by_level[i];
    for (std::size_t i = 0; i < by_status_class.size(); ++i) {
        by_status_class[i] += other.by_status_class[i];
    }

    for (const auto& [k, v] : other.by_status) by_status[k] += v;
    for (const auto& [k, v] : other.by_service) by_service[k] += v;
    for (const auto& [k, v] : other.error_messages) error_messages[k] += v;
    for (const auto& [k, v] : other.endpoints) endpoints[k].merge(v);
    for (const auto& [k, v] : other.failures_by_service) failures_by_service[k] += v;

    time_buckets.merge(other.time_buckets);
}

} // namespace la
