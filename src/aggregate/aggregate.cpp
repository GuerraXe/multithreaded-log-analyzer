#include "aggregate/aggregate.hpp"

#include <algorithm>
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
        endpoints[key].observe(r.duration_us, collect_durations);
    }

    const bool failure = is_failure(r);
    if (failure) ++failures_by_service[std::string(r.service)];
    time_buckets.add(r.epoch_ms, failure);
}

void Aggregate::note_malformed(std::uint64_t line, ParseError reason, std::string_view raw) {
    ++malformed;
    if (malformed_samples.size() >= sample_limit) return;

    if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
    if (raw.size() > 256) raw = raw.substr(0, 256);
    // Scan is in file order, so appending keeps the vector sorted by line.
    malformed_samples.push_back({line, reason, std::string(raw)});
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

    // Merge two line-sorted sample lists, keep the earliest up to the limit.
    std::vector<MalformedSample> merged;
    const std::size_t limit = std::max(sample_limit, other.sample_limit);
    merged.reserve(std::min(limit, malformed_samples.size() + other.malformed_samples.size()));
    std::size_t i = 0, j = 0;
    while (merged.size() < limit &&
           (i < malformed_samples.size() || j < other.malformed_samples.size())) {
        const bool take_left =
            j >= other.malformed_samples.size() ||
            (i < malformed_samples.size() &&
             malformed_samples[i].line <= other.malformed_samples[j].line);
        merged.push_back(take_left ? malformed_samples[i++] : other.malformed_samples[j++]);
    }
    malformed_samples = std::move(merged);
    sample_limit = limit;
}

} // namespace la
