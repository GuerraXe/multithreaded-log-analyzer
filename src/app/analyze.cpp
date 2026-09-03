#include "app/analyze.hpp"

#include "parse/formats.hpp"

#include <cstddef>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace la {
namespace {

// Invoke `fn` once per line in `buf`, splitting on '\n'. A trailing '\n' does
// not yield a final empty line; a missing trailing '\n' still yields the last
// line. This is the temporary M1 line iterator; M5 replaces it with the
// memory-mapped, chunk-aware iterator in the `io` module.
template <typename F>
void for_each_line(std::string_view buf, F&& fn) {
    std::size_t start = 0;
    while (start < buf.size()) {
        const std::size_t nl = buf.find('\n', start);
        if (nl == std::string_view::npos) {
            fn(buf.substr(start));
            return;
        }
        fn(buf.substr(start, nl - start));
        start = nl + 1;
    }
}

} // namespace

AnalyzeSummary analyze_buffer(std::string_view buffer, const ILogFormat& fmt) {
    AnalyzeSummary s;
    s.bytes = buffer.size();
    for_each_line(buffer, [&](std::string_view line) {
        if (is_blank_line(line)) {
            ++s.blank;
            return;
        }
        ++s.lines;
        if (fmt.parse_line(line).ok) {
            ++s.records;
        } else {
            ++s.malformed;
        }
    });
    return s;
}

int run_analyze(const std::string& path, std::ostream& out, std::ostream& err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err << "loganalyzer: cannot open '" << path << "'\n";
        return 2;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string buf = ss.str();

    const auto fmt = make_log_format("pipe");
    const AnalyzeSummary s = analyze_buffer(buf, *fmt);

    out << "file        " << path << "\n"
        << "bytes       " << s.bytes << "\n"
        << "lines       " << s.lines << "\n"
        << "records     " << s.records << "\n"
        << "malformed   " << s.malformed << "\n"
        << "blank       " << s.blank << "\n";
    return 0;
}

} // namespace la
