#include "support/test_framework.hpp"

#include "parse/log_record.hpp"
#include "parse/pipe_format.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

using namespace la;

namespace {

const PipeDelimitedFormat kFmt;

// LogRecord string fields are views into the parsed buffer, so a test must
// keep that buffer alive while it inspects the result. `Parsed` owns the line
// text and the result together. (Member declaration order matters: `line` is
// constructed before `r`, which parses it.)
struct Parsed {
    std::string line;
    ParseResult r;
    explicit Parsed(std::string l) : line(std::move(l)), r(kFmt.parse_line(line)) {}
};

ParseError error_of(std::string_view line) {
    // Error path never touches record string fields, so a temporary is fine.
    return kFmt.parse_line(line).error;
}

bool parses_ok(std::string_view line) { return kFmt.parse_line(line).ok; }

// Build a line with exactly six " | " separators, so field-count is never in
// question and each field's own validation is what a test exercises.
std::string line6(std::string_view ts, std::string_view lvl, std::string_view svc,
                  std::string_view req, std::string_view st, std::string_view du,
                  std::string_view msg) {
    std::string s;
    for (std::string_view part : {ts, lvl, svc, req, st, du}) {
        s += part;
        s += " | ";
    }
    s += msg;
    return s;
}

constexpr std::string_view kTs = "2000-01-01T00:00:00Z";

} // namespace

TEST_CASE("pipe: fully populated HTTP line") {
    const Parsed p{line6("2000-01-01T00:00:00.789Z", "INFO", "api-gateway",
                         "GET /v1/users/42", "200", "13.4", "request completed")};
    CHECK(p.r.ok);
    CHECK_EQ(p.r.record.epoch_ms, std::int64_t{946'684'800'789});
    CHECK(p.r.record.level == Level::Info);
    CHECK_EQ(std::string(p.r.record.service), std::string("api-gateway"));
    CHECK(p.r.record.method == Method::Get);
    CHECK_EQ(std::string(p.r.record.path), std::string("/v1/users/42"));
    CHECK_EQ(p.r.record.status, std::uint16_t{200});
    CHECK_EQ(p.r.record.duration_us, std::int64_t{13'400}); // 13.4 ms
    CHECK_EQ(std::string(p.r.record.message), std::string("request completed"));
}

TEST_CASE("pipe: non-HTTP line leaves request/status/duration unset") {
    const Parsed p{line6(kTs, "WARN", "scheduler", "", "", "", "queue depth high")};
    CHECK(p.r.ok);
    CHECK(p.r.record.level == Level::Warn);
    CHECK(p.r.record.method == Method::None);
    CHECK(p.r.record.path.empty());
    CHECK_EQ(p.r.record.status, std::uint16_t{0});
    CHECK_EQ(p.r.record.duration_us, std::int64_t{-1});
    CHECK_EQ(std::string(p.r.record.message), std::string("queue depth high"));
}

TEST_CASE("pipe: every level name, case-insensitively") {
    struct Case { std::string_view text; Level level; };
    for (const Case c : {Case{"TRACE", Level::Trace}, Case{"debug", Level::Debug},
                         Case{"InFo", Level::Info}, Case{"WARN", Level::Warn},
                         Case{"error", Level::Error}, Case{"FATAL", Level::Fatal}}) {
        const Parsed p{line6(kTs, c.text, "svc", "", "", "", "m")};
        CHECK(p.r.ok);
        CHECK(p.r.record.level == c.level);
    }
}

TEST_CASE("pipe: message may contain a bare pipe") {
    const Parsed p{line6(kTs, "INFO", "svc", "", "", "", "a|b|c")};
    CHECK(p.r.ok);
    CHECK_EQ(std::string(p.r.record.message), std::string("a|b|c"));
}

TEST_CASE("pipe: message absorbs further ' | ' sequences (documented limitation)") {
    const Parsed p{line6(kTs, "INFO", "svc", "", "", "", "left | right")};
    CHECK(p.r.ok);
    CHECK_EQ(std::string(p.r.record.message), std::string("left | right"));
}

TEST_CASE("pipe: empty message is allowed") {
    const Parsed p{line6(kTs, "INFO", "svc", "", "", "", "")};
    CHECK(p.r.ok);
    CHECK(p.r.record.message.empty());
}

TEST_CASE("pipe: trailing CR from CRLF input is stripped") {
    const Parsed p{line6(kTs, "INFO", "svc", "", "", "", "hello") + "\r"};
    CHECK(p.r.ok);
    CHECK_EQ(std::string(p.r.record.message), std::string("hello"));
}

TEST_CASE("pipe: surrounding whitespace on fields is trimmed") {
    const Parsed p{line6("  2000-01-01T00:00:00Z  ", "  INFO  ", "  svc  ",
                         "  GET /x  ", "  200  ", "  13.4  ", "  hi  ")};
    CHECK(p.r.ok);
    CHECK_EQ(std::string(p.r.record.service), std::string("svc"));
    CHECK_EQ(std::string(p.r.record.path), std::string("/x")); // field trimmed before split
    CHECK_EQ(p.r.record.status, std::uint16_t{200});
    CHECK_EQ(p.r.record.duration_us, std::int64_t{13'400});
    CHECK_EQ(std::string(p.r.record.message), std::string("hi"));
}

TEST_CASE("pipe: too few fields is a field-count error") {
    CHECK(error_of("a | b | c") == ParseError::FieldCount);
    CHECK(error_of("") == ParseError::FieldCount);
    CHECK(!parses_ok("a | b | c"));
}

TEST_CASE("pipe: bad timestamp is reported") {
    CHECK(error_of(line6("nope", "INFO", "svc", "", "", "", "m")) == ParseError::Timestamp);
}

TEST_CASE("pipe: unknown level is reported") {
    CHECK(error_of(line6(kTs, "NOTICE", "svc", "", "", "", "m")) == ParseError::Level);
}

TEST_CASE("pipe: service character set is enforced") {
    CHECK(error_of(line6(kTs, "INFO", "api gateway", "", "", "", "m")) == ParseError::Service);
    CHECK(error_of(line6(kTs, "INFO", "api/gateway", "", "", "", "m")) == ParseError::Service);
    CHECK(error_of(line6(kTs, "INFO", "", "", "", "", "m")) == ParseError::Service);
    CHECK(parses_ok(line6(kTs, "INFO", "api-gw_1.2", "", "", "", "m")));
}

TEST_CASE("pipe: request grammar is enforced") {
    CHECK(error_of(line6(kTs, "INFO", "svc", "GET", "", "", "m")) == ParseError::Request);
    CHECK(error_of(line6(kTs, "INFO", "svc", "GET x", "", "", "m")) == ParseError::Request);
    CHECK(error_of(line6(kTs, "INFO", "svc", "FETCH /x", "", "", "m")) == ParseError::Request);
    CHECK(error_of(line6(kTs, "INFO", "svc", "get /x", "", "", "m")) == ParseError::Request);

    const Parsed ok{line6(kTs, "INFO", "svc", "DELETE /a/b", "", "", "m")};
    CHECK(ok.r.ok);
    CHECK(ok.r.record.method == Method::Delete);
    CHECK_EQ(std::string(ok.r.record.path), std::string("/a/b"));
}

TEST_CASE("pipe: status must be an integer in [100,599] when present") {
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "99", "", "m")) == ParseError::Status);
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "600", "", "m")) == ParseError::Status);
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "20x", "", "m")) == ParseError::Status);
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "1000", "", "m")) == ParseError::Status);
    CHECK(parses_ok(line6(kTs, "INFO", "svc", "", "200", "", "m")));
}

TEST_CASE("pipe: duration parsing and rejection") {
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "", "-1", "m")) == ParseError::Duration);
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "", "abc", "m")) == ParseError::Duration);
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "", "1e3", "m")) == ParseError::Duration);
    CHECK(error_of(line6(kTs, "INFO", "svc", "", "", "1.2.3", "m")) == ParseError::Duration);

    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "0", "m")}.r.record.duration_us,
             std::int64_t{0});
    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "12", "m")}.r.record.duration_us,
             std::int64_t{12'000}); // 12 ms
    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "13.4", "m")}.r.record.duration_us,
             std::int64_t{13'400}); // 13.4 ms
    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "0.25", "m")}.r.record.duration_us,
             std::int64_t{250}); // 0.25 ms
}

TEST_CASE("pipe: sub-microsecond duration rounds half to even") {
    // The 4th fractional (ms) digit is the decider; exact ties go to even.
    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "0.0005", "m")}.r.record.duration_us,
             std::int64_t{0}); // 0 us is even -> stays
    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "0.0015", "m")}.r.record.duration_us,
             std::int64_t{2}); // 1 us is odd -> up to 2
    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "0.0025", "m")}.r.record.duration_us,
             std::int64_t{2}); // 2 us is even -> stays
    CHECK_EQ(Parsed{line6(kTs, "INFO", "svc", "", "", "0.00051", "m")}.r.record.duration_us,
             std::int64_t{1}); // past the tie -> up to 1
}
