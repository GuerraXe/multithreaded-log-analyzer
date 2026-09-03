#include "support/test_framework.hpp"

#include "cli/parser.hpp"
#include "filter/record_filter.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

using namespace la;

namespace {
ArgParse cli(std::initializer_list<const char*> args) {
    std::vector<char*> v;
    for (const char* a : args) v.push_back(const_cast<char*>(a));
    return parse_args(static_cast<int>(v.size()), v.data());
}
} // namespace

TEST_CASE("cli: no command is an error") {
    const auto r = cli({"loganalyzer"});
    CHECK(!r.ok);
    CHECK(!r.error.empty());
}

TEST_CASE("cli: version and help") {
    CHECK(cli({"loganalyzer", "version"}).options.command == Command::Version);
    CHECK(cli({"loganalyzer", "--version"}).options.command == Command::Version);
    const auto h = cli({"loganalyzer", "help", "analyze"});
    CHECK(h.ok);
    CHECK(h.options.command == Command::Help);
    CHECK_EQ(h.options.help_topic, std::string("analyze"));
}

TEST_CASE("cli: unknown command") {
    const auto r = cli({"loganalyzer", "frobnicate", "x"});
    CHECK(!r.ok);
}

TEST_CASE("cli: analyze requires exactly one file") {
    CHECK(!cli({"loganalyzer", "analyze"}).ok);
    CHECK(!cli({"loganalyzer", "analyze", "a.log", "b.log"}).ok);
    const auto r = cli({"loganalyzer", "analyze", "server.log"});
    CHECK(r.ok);
    CHECK(r.options.command == Command::Analyze);
    CHECK_EQ(r.options.input_path, std::string("server.log"));
}

TEST_CASE("cli: analyze defaults") {
    const auto o = cli({"loganalyzer", "analyze", "f"}).options;
    CHECK_EQ(o.threads, 1);
    CHECK_EQ(o.top_n, 10);
    CHECK_EQ(o.interval_ms, std::int64_t{60'000});
    CHECK(o.report == ReportFormat::Text);
    CHECK(!o.strict);
    CHECK_EQ(o.show_malformed, 5);
    CHECK_EQ(o.format, std::string("pipe"));
    CHECK(RecordFilter{o.filter}.is_pass_through());
}

TEST_CASE("cli: --threads validation") {
    CHECK_EQ(cli({"loganalyzer", "analyze", "--threads", "8", "f"}).options.threads, 8);
    CHECK_EQ(cli({"loganalyzer", "analyze", "--threads", "0", "f"}).options.threads, 0);
    CHECK(!cli({"loganalyzer", "analyze", "--threads", "x", "f"}).ok);
    CHECK(!cli({"loganalyzer", "analyze", "--threads", "-2", "f"}).ok);
    CHECK(!cli({"loganalyzer", "analyze", "--threads"}).ok); // missing value
}

TEST_CASE("cli: level filters") {
    CHECK(cli({"loganalyzer", "analyze", "--level", "warn", "f"}).options.filter.min_level ==
          Level::Warn);
    CHECK(!cli({"loganalyzer", "analyze", "--level", "bogus", "f"}).ok);

    const auto o = cli({"loganalyzer", "analyze", "--level", "info",
                        "--level-only", "INFO,ERROR", "f"}).options;
    CHECK_EQ(o.filter.level_only.size(), std::size_t{2});
    CHECK(!o.filter.min_level.has_value()); // --level-only clears --level
}

TEST_CASE("cli: time bounds") {
    const auto o = cli({"loganalyzer", "analyze",
                        "--from", "2020-01-01T00:00:00Z",
                        "--to", "2020-02-01T00:00:00Z", "f"}).options;
    CHECK(o.filter.from_ms.has_value());
    CHECK(o.filter.to_ms.has_value());
    CHECK(*o.filter.from_ms < *o.filter.to_ms);
    CHECK(!cli({"loganalyzer", "analyze", "--from", "nonsense", "f"}).ok);
}

TEST_CASE("cli: status-class accepts Nxx and N, rejects others") {
    const auto o = cli({"loganalyzer", "analyze",
                        "--status-class", "2xx", "--status-class", "5", "f"}).options;
    CHECK_EQ(o.filter.status_classes.size(), std::size_t{2});
    CHECK_EQ(o.filter.status_classes[0], 2);
    CHECK_EQ(o.filter.status_classes[1], 5);
    CHECK(!cli({"loganalyzer", "analyze", "--status-class", "9xx", "f"}).ok);
    CHECK(!cli({"loganalyzer", "analyze", "--status-class", "20", "f"}).ok);
}

TEST_CASE("cli: repeated --service accumulates") {
    const auto o = cli({"loganalyzer", "analyze",
                        "--service", "api", "--service", "db", "f"}).options;
    CHECK_EQ(o.filter.services.size(), std::size_t{2});
}

TEST_CASE("cli: --interval units") {
    CHECK_EQ(cli({"loganalyzer", "analyze", "--interval", "30s", "f"}).options.interval_ms,
             std::int64_t{30'000});
    CHECK_EQ(cli({"loganalyzer", "analyze", "--interval", "5m", "f"}).options.interval_ms,
             std::int64_t{300'000});
    CHECK_EQ(cli({"loganalyzer", "analyze", "--interval", "1h", "f"}).options.interval_ms,
             std::int64_t{3'600'000});
    CHECK_EQ(cli({"loganalyzer", "analyze", "--interval", "90", "f"}).options.interval_ms,
             std::int64_t{90'000});
    CHECK(!cli({"loganalyzer", "analyze", "--interval", "0", "f"}).ok);
    CHECK(!cli({"loganalyzer", "analyze", "--interval", "abc", "f"}).ok);
}

TEST_CASE("cli: --report and --top") {
    CHECK(cli({"loganalyzer", "analyze", "--report", "json", "f"}).options.report ==
          ReportFormat::Json);
    CHECK(!cli({"loganalyzer", "analyze", "--report", "xml", "f"}).ok);
    CHECK_EQ(cli({"loganalyzer", "analyze", "--top", "5", "f"}).options.top_n, 5);
    CHECK(!cli({"loganalyzer", "analyze", "--top", "0", "f"}).ok);
}

TEST_CASE("cli: flags and output") {
    const auto o = cli({"loganalyzer", "analyze", "--exact-percentiles", "--strict",
                        "--no-color", "-o", "out.txt", "f"}).options;
    CHECK(o.exact_percentiles);
    CHECK(o.strict);
    CHECK(o.no_color);
    CHECK_EQ(o.output_path, std::string("out.txt"));
}

TEST_CASE("cli: unknown option is rejected") {
    CHECK(!cli({"loganalyzer", "analyze", "--wat", "f"}).ok);
}
