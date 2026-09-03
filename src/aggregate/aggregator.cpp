#include "aggregate/aggregator.hpp"

#include "core/lines.hpp"

namespace la {

Aggregate aggregate_buffer(std::string_view buffer, const ILogFormat& fmt,
                           const RecordFilter& filter, const AggregateOptions& opt) {
    Aggregate agg(opt.interval_ms, opt.malformed_sample_limit, opt.collect_durations);
    agg.bytes = buffer.size();

    std::uint64_t line_no = opt.line_number_base - 1;
    for_each_line(buffer, [&](std::string_view line) {
        ++line_no;
        if (is_blank_line(line)) {
            ++agg.blank;
            return;
        }
        ++agg.total_lines;
        const ParseResult r = fmt.parse_line(line);
        if (!r.ok) {
            agg.note_malformed(line_no, r.error, line);
            return;
        }
        ++agg.records;
        if (filter.matches(r.record)) agg.observe(r.record);
    });

    return agg;
}

Aggregate aggregate_buffer(std::string_view buffer, const ILogFormat& fmt,
                           const RecordFilter& filter, std::int64_t interval_ms) {
    AggregateOptions opt;
    opt.interval_ms = interval_ms;
    return aggregate_buffer(buffer, fmt, filter, opt);
}

} // namespace la
