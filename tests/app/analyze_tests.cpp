#include "support/test_framework.hpp"

#include "app/analyze.hpp"
#include "filter/record_filter.hpp"
#include "parse/pipe_format.hpp"

#include <cstdint>
#include <string>

using namespace la;

namespace {
const PipeDelimitedFormat kFmt;
const RecordFilter kPassThrough{FilterSpec{}};

// A line whose service is `svc` and level is `lvl`.
std::string line(std::string_view lvl, std::string_view svc, std::string_view msg) {
    std::string s = "2000-01-01T00:00:00Z | ";
    s += lvl;
    s += " | ";
    s += svc;
    s += " |  |  |  | ";
    s += msg;
    return s;
}
} // namespace

TEST_CASE("analyze_buffer: mixed valid / malformed / blank lines") {
    std::string buf;
    buf += line("INFO", "svc", "one") + "\n";
    buf += line("INFO", "svc", "two") + "\n";
    buf += "this | is | malformed\n"; // too few fields
    buf += "\n";                      // blank
    buf += "   \t  \n";               // whitespace only
    buf += line("INFO", "svc", "three"); // no trailing newline

    const AnalyzeSummary s = analyze_buffer(buf, kFmt, kPassThrough);
    CHECK_EQ(s.records, std::uint64_t{3});
    CHECK_EQ(s.kept, std::uint64_t{3}); // pass-through filter keeps all
    CHECK_EQ(s.malformed, std::uint64_t{1});
    CHECK_EQ(s.blank, std::uint64_t{2});
    CHECK_EQ(s.lines, std::uint64_t{4}); // 3 valid + 1 malformed
    CHECK_EQ(s.bytes, static_cast<std::uint64_t>(buf.size()));
}

TEST_CASE("analyze_buffer: empty buffer yields all zeros") {
    const AnalyzeSummary s = analyze_buffer("", kFmt, kPassThrough);
    CHECK_EQ(s.records, std::uint64_t{0});
    CHECK_EQ(s.kept, std::uint64_t{0});
    CHECK_EQ(s.malformed, std::uint64_t{0});
    CHECK_EQ(s.blank, std::uint64_t{0});
    CHECK_EQ(s.lines, std::uint64_t{0});
    CHECK_EQ(s.bytes, std::uint64_t{0});
}

TEST_CASE("analyze_buffer: only newlines are all blank") {
    const AnalyzeSummary s = analyze_buffer("\n\n\n", kFmt, kPassThrough);
    CHECK_EQ(s.blank, std::uint64_t{3});
    CHECK_EQ(s.lines, std::uint64_t{0});
}

TEST_CASE("analyze_buffer: single line without trailing newline is counted") {
    const AnalyzeSummary s = analyze_buffer(line("INFO", "svc", "solo"), kFmt, kPassThrough);
    CHECK_EQ(s.records, std::uint64_t{1});
    CHECK_EQ(s.kept, std::uint64_t{1});
    CHECK_EQ(s.lines, std::uint64_t{1});
}

TEST_CASE("analyze_buffer: filter narrows kept without changing records") {
    std::string buf;
    buf += line("INFO", "api", "a") + "\n";
    buf += line("ERROR", "api", "b") + "\n";
    buf += line("ERROR", "db", "c") + "\n";

    FilterSpec spec;
    spec.min_level = Level::Error;
    spec.services = {"api"};
    const RecordFilter filter{spec};

    const AnalyzeSummary s = analyze_buffer(buf, kFmt, filter);
    CHECK_EQ(s.records, std::uint64_t{3});
    CHECK_EQ(s.kept, std::uint64_t{1}); // only ERROR + api
}
