#include "support/test_framework.hpp"

#include "app/analyze.hpp"
#include "parse/pipe_format.hpp"

#include <cstdint>
#include <string>

using namespace la;

namespace {
const PipeDelimitedFormat kFmt;

std::string valid_line(std::string_view msg) {
    std::string s = "2000-01-01T00:00:00Z | INFO | svc |  |  |  | ";
    s += msg;
    return s;
}
} // namespace

TEST_CASE("analyze_buffer: mixed valid / malformed / blank lines") {
    std::string buf;
    buf += valid_line("one") + "\n";
    buf += valid_line("two") + "\n";
    buf += "this | is | malformed\n"; // too few fields
    buf += "\n";                      // blank
    buf += "   \t  \n";               // whitespace only
    buf += valid_line("three");       // no trailing newline

    const AnalyzeSummary s = analyze_buffer(buf, kFmt);
    CHECK_EQ(s.records, std::uint64_t{3});
    CHECK_EQ(s.malformed, std::uint64_t{1});
    CHECK_EQ(s.blank, std::uint64_t{2});
    CHECK_EQ(s.lines, std::uint64_t{4}); // 3 valid + 1 malformed
    CHECK_EQ(s.bytes, static_cast<std::uint64_t>(buf.size()));
}

TEST_CASE("analyze_buffer: empty buffer yields all zeros") {
    const AnalyzeSummary s = analyze_buffer("", kFmt);
    CHECK_EQ(s.records, std::uint64_t{0});
    CHECK_EQ(s.malformed, std::uint64_t{0});
    CHECK_EQ(s.blank, std::uint64_t{0});
    CHECK_EQ(s.lines, std::uint64_t{0});
    CHECK_EQ(s.bytes, std::uint64_t{0});
}

TEST_CASE("analyze_buffer: only newlines are all blank") {
    const AnalyzeSummary s = analyze_buffer("\n\n\n", kFmt);
    CHECK_EQ(s.blank, std::uint64_t{3});
    CHECK_EQ(s.lines, std::uint64_t{0});
}

TEST_CASE("analyze_buffer: single line without trailing newline is counted") {
    const AnalyzeSummary s = analyze_buffer(valid_line("solo"), kFmt);
    CHECK_EQ(s.records, std::uint64_t{1});
    CHECK_EQ(s.lines, std::uint64_t{1});
}
