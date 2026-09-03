#include "support/test_framework.hpp"

#include "filter/record_filter.hpp"
#include "parse/log_record.hpp"

#include <cstdint>
#include <string_view>

using namespace la;

namespace {

// String fields point at string literals (static storage), so records built
// here stay valid for the whole test.
LogRecord rec(std::int64_t ms, Level lv, std::string_view svc, Method m,
              std::string_view path, std::uint16_t status) {
    LogRecord r;
    r.epoch_ms = ms;
    r.level = lv;
    r.service = svc;
    r.method = m;
    r.path = path;
    r.status = status;
    return r;
}

const LogRecord kInfoApi = rec(1000, Level::Info, "api", Method::Get, "/v1/users", 200);
const LogRecord kErrApi = rec(2000, Level::Error, "api", Method::Post, "/v1/checkout", 500);
const LogRecord kErrDb = rec(3000, Level::Error, "db", Method::None, "", 0);

} // namespace

TEST_CASE("filter: default spec is pass-through") {
    const RecordFilter f{FilterSpec{}};
    CHECK(f.is_pass_through());
    CHECK(f.matches(kInfoApi));
    CHECK(f.matches(kErrDb));
}

TEST_CASE("filter: time range is half-open [from, to)") {
    FilterSpec s;
    s.from_ms = 1000;
    s.to_ms = 3000;
    const RecordFilter f{s};
    CHECK(!f.is_pass_through());
    CHECK(f.matches(rec(1000, Level::Info, "x", Method::None, "", 0)));  // from is inclusive
    CHECK(f.matches(rec(2999, Level::Info, "x", Method::None, "", 0)));
    CHECK(!f.matches(rec(3000, Level::Info, "x", Method::None, "", 0))); // to is exclusive
    CHECK(!f.matches(rec(999, Level::Info, "x", Method::None, "", 0)));
}

TEST_CASE("filter: --level is a minimum severity threshold") {
    FilterSpec s;
    s.min_level = Level::Warn;
    const RecordFilter f{s};
    CHECK(!f.matches(rec(0, Level::Info, "x", Method::None, "", 0)));
    CHECK(f.matches(rec(0, Level::Warn, "x", Method::None, "", 0)));
    CHECK(f.matches(rec(0, Level::Error, "x", Method::None, "", 0)));
}

TEST_CASE("filter: --level-only is an exact set") {
    FilterSpec s;
    s.level_only = {Level::Info, Level::Error};
    const RecordFilter f{s};
    CHECK(f.matches(rec(0, Level::Info, "x", Method::None, "", 0)));
    CHECK(!f.matches(rec(0, Level::Warn, "x", Method::None, "", 0)));
    CHECK(f.matches(rec(0, Level::Error, "x", Method::None, "", 0)));
}

TEST_CASE("filter: services combine with OR") {
    FilterSpec s;
    s.services = {"api", "db"};
    const RecordFilter f{s};
    CHECK(f.matches(kInfoApi));
    CHECK(f.matches(kErrDb));
    CHECK(!f.matches(rec(0, Level::Info, "cache", Method::None, "", 0)));
}

TEST_CASE("filter: status classes combine with OR; missing status is excluded") {
    FilterSpec s;
    s.status_classes = {5};
    const RecordFilter f{s};
    CHECK(f.matches(kErrApi));                                            // 500
    CHECK(f.matches(rec(0, Level::Info, "x", Method::Get, "/", 502)));
    CHECK(!f.matches(kInfoApi));                                          // 200
    CHECK(!f.matches(kErrDb));                                            // no status
}

TEST_CASE("filter: status classes 2xx and 4xx") {
    FilterSpec s;
    s.status_classes = {2, 4};
    const RecordFilter f{s};
    CHECK(f.matches(kInfoApi));                                       // 200
    CHECK(f.matches(rec(0, Level::Info, "x", Method::Get, "/", 404)));
    CHECK(!f.matches(kErrApi));                                       // 500
}

TEST_CASE("filter: path prefix; records without a path are excluded") {
    FilterSpec s;
    s.path_prefix = "/v1";
    const RecordFilter f{s};
    CHECK(f.matches(kInfoApi));   // /v1/users
    CHECK(!f.matches(rec(0, Level::Info, "x", Method::Get, "/v2/x", 200)));
    CHECK(!f.matches(kErrDb));    // no path
}

TEST_CASE("filter: path substring match") {
    FilterSpec s;
    s.path_contains = "checkout";
    const RecordFilter f{s};
    CHECK(f.matches(kErrApi));    // /v1/checkout
    CHECK(!f.matches(kInfoApi));  // /v1/users
}

TEST_CASE("filter: categories combine with AND") {
    FilterSpec s;
    s.min_level = Level::Error;
    s.services = {"api"};
    const RecordFilter f{s};
    CHECK(f.matches(kErrApi));    // Error + api
    CHECK(!f.matches(kErrDb));    // Error + db
    CHECK(!f.matches(kInfoApi));  // Info + api
}
