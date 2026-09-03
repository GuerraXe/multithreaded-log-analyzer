#include "app/commands.hpp"

#include "bench/benchmark.hpp"
#include "concurrency/parallel_aggregate.hpp"
#include "filter/record_filter.hpp"
#include "io/mapped_file.hpp"
#include "parse/formats.hpp"

#include <ostream>
#include <string_view>

namespace la {

int run_benchmark(const Options& opt, std::ostream& out, std::ostream& err) {
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

    BenchmarkOptions bopt;
    bopt.repeat = opt.repeat;
    bopt.warmup = opt.warmup;
    bopt.aggregate.interval_ms = opt.interval_ms;
    bopt.aggregate.malformed_sample_limit = 0; // samples irrelevant to timing
    for (const int t : opt.threads_list) {
        bopt.thread_counts.push_back(resolve_thread_count(t < 0 ? 0u : static_cast<unsigned>(t)));
    }

    const RecordFilter filter(opt.filter);
    const BenchmarkReport report = run_benchmark_core(buf, *fmt, filter, bopt);

    if (opt.report == ReportFormat::Json) {
        render_benchmark_json(report, out);
    } else {
        render_benchmark_text(report, out);
    }
    return 0;
}

} // namespace la
