#include "app/analyze.hpp"

#include "aggregate/aggregator.hpp"
#include "core/lines.hpp"
#include "parse/formats.hpp"
#include "report/text_renderer.hpp"
#include "stats/report.hpp"

#include <fstream>
#include <ostream>
#include <sstream>
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
    std::ifstream in(opt.input_path, std::ios::binary);
    if (!in) {
        err << "loganalyzer: cannot open '" << opt.input_path << "'\n";
        return 2;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string buf = ss.str();

    const auto fmt = make_log_format(opt.format);
    if (!fmt) {
        err << "loganalyzer: unknown format '" << opt.format << "'\n";
        return 1;
    }

    const RecordFilter filter(opt.filter);
    const Aggregate agg = aggregate_buffer(buf, *fmt, filter, opt.interval_ms);
    const Report rep = build_report(agg, opt.top_n);

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

    render_text(rep, *dst); // JSON path added in M4
    return 0;
}

} // namespace la
