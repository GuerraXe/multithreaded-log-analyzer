#include "app/run.hpp"

#include "app/analyze.hpp"
#include "app/commands.hpp"
#include "core/version.hpp"

#include <ostream>

namespace la {
namespace {

void print_usage(std::ostream& os) {
    os << version_string() << "\n\n"
       << "usage: loganalyzer <command> [options]\n\n"
       << "commands:\n"
       << "  analyze <file>     parse a log file and print a statistics report\n"
       << "  benchmark <file>   compare sequential vs multithreaded throughput\n"
       << "  gen <file>         generate a synthetic dataset (seeded)\n"
       << "  version            print version and exit\n"
       << "  help [command]     show help\n\n"
       << "analyze options:\n"
       << "  --threads N            worker threads (default 1; 0 = all cores)\n"
       << "  --format NAME          log format (default: pipe)\n"
       << "  --from <ts> / --to <ts>   ISO-8601 bounds, half-open [from, to)\n"
       << "  --level <LVL>          minimum severity (TRACE..FATAL)\n"
       << "  --level-only <L,L>     exact severity set (overrides --level)\n"
       << "  --service <name>       filter by service (repeatable)\n"
       << "  --status-class <Nxx>   1xx..5xx (repeatable)\n"
       << "  --path-prefix <p>      endpoint path prefix\n"
       << "  --path-contains <s>    endpoint path substring\n"
       << "  --top N                ranked-table size (default 10)\n"
       << "  --interval <dur>       time bucket: 30s | 1m | 5m | 1h (default 1m)\n"
       << "  --report <fmt>         text | json (default text)\n"
       << "  --strict              exit 3 if any malformed lines\n"
       << "  --show-malformed N     malformed samples to print (default 5)\n"
       << "  -o, --output <file>    write report to file instead of stdout\n";
}

} // namespace

int run(const Options& opt, std::ostream& out, std::ostream& err) {
    switch (opt.command) {
        case Command::Version:
            out << version_string() << '\n';
            return 0;

        case Command::Help:
            print_usage(out);
            return 0;

        case Command::Analyze:
            return run_analyze(opt, out, err);

        case Command::Benchmark:
            return run_benchmark(opt, out, err);

        case Command::Gen:
            return run_gen(opt, out, err);

        case Command::None:
            err << "loganalyzer: no command\n";
            return 1;
    }
    return 1;
}

} // namespace la
