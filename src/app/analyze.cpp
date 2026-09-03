#include "app/analyze.hpp"

#include "aggregate/aggregator.hpp"
#include "concurrency/parallel_aggregate.hpp"
#include "core/lines.hpp"
#include "io/mapped_file.hpp"
#include "parse/formats.hpp"
#include "report/json_renderer.hpp"
#include "report/text_renderer.hpp"
#include "stats/report.hpp"

#include <cstddef>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>

namespace la {

AnalyzeSummary analyze_buffer(std::string_view buffer, const ILogFormat& fmt,
                              const RecordFilter& filter) {
    AnalyzeSummary s;
    s.bytes = buffer.size();
    for_each_line(buffer, [&](std::string_view line) {
        if (is_blank_line(line)) {
            ++s.blank;
            return;
        }
        ++s.lines;
        const ParseResult r = fmt.parse_line(line);
        if (!r.ok) {
            ++s.malformed;
            return;
        }
        ++s.records;
        if (filter.matches(r.record)) ++s.kept;
    });
    return s;
}

int run_analyze(const Options& opt, std::ostream& out, std::ostream& err) {
    const MappedFile file = MappedFile::open(opt.input_path);
    if (!file) {
        err << "loganalyzer: " << file.error() << "\n";
        return 2;
    }
    const std::string_view buf = file.data();

    const auto fmt = make_log_format(opt.format);
    if (!fmt) {
        err << "loganalyzer: unknown format '" << opt.format << "'\n";
        return 1;
    }

    unsigned threads = opt.threads < 0 ? 1u : static_cast<unsigned>(opt.threads);
    if (opt.exact_percentiles && threads != 1) {
        err << "loganalyzer: --exact-percentiles forces single-threaded analysis\n";
        threads = 1;
    }

    AggregateOptions aopt;
    aopt.interval_ms = opt.interval_ms;
    aopt.malformed_sample_limit =
        opt.show_malformed >= 0 ? static_cast<std::size_t>(opt.show_malformed) : 0;
    aopt.collect_durations = opt.exact_percentiles;

    const RecordFilter filter(opt.filter);
    const Aggregate agg = parallel_aggregate(buf, *fmt, filter, aopt, threads);
    const Report rep = build_report(agg, opt.top_n, opt.exact_percentiles);

    std::ofstream fout;
    std::ostream* dst = &out;
    if (!opt.output_path.empty()) {
        fout.open(opt.output_path, std::ios::binary);
        if (!fout) {
            err << "loganalyzer: cannot write '" << opt.output_path << "'\n";
            return 2;
        }
        dst = &fout;
    }

    if (opt.report == ReportFormat::Json) {
        render_json(rep, *dst);
    } else {
        render_text(rep, *dst);
    }

    if (opt.strict && agg.malformed > 0) {
        err << "loganalyzer: " << agg.malformed
            << " malformed line(s) present; failing due to --strict\n";
        return 3;
    }
    return 0;
}

} // namespace la
