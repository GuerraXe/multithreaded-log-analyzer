#include "aggregate/aggregator.hpp"

#include "core/lines.hpp"

namespace la {

Aggregate aggregate_buffer(std::string_view buffer, const ILogFormat& fmt,
                           const RecordFilter& filter, std::int64_t interval_ms) {
    Aggregate agg(interval_ms);
    agg.bytes = buffer.size();

    for_each_line(buffer, [&](std::string_view line) {
        if (is_blank_line(line)) {
            ++agg.blank;
            return;
        }
        ++agg.total_lines;
        const ParseResult r = fmt.parse_line(line);
        if (!r.ok) {
            ++agg.malformed;
            return;
        }
        ++agg.records;
        if (filter.matches(r.record)) agg.observe(r.record);
    });

    return agg;
}

} // namespace la
