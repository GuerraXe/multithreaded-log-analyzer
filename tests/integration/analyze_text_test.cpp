#include "support/test_framework.hpp"

#include "app/run.hpp"
#include "cli/options.hpp"

#include <fstream>
#include <sstream>
#include <string>

using namespace la;

namespace {

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

#ifndef LA_DATA_DIR
#define LA_DATA_DIR "."
#endif
const std::string kDataDir = LA_DATA_DIR;

} // namespace

TEST_CASE("integration: analyze text report matches the golden file") {
    Options opt;
    opt.command = Command::Analyze;
    opt.input_path = kDataDir + "/valid_small.log";

    std::ostringstream out;
    std::ostringstream err;
    const int code = run(opt, out, err);

    CHECK_EQ(code, 0);
    CHECK(err.str().empty());

    const std::string expected = slurp(kDataDir + "/valid_small.report.txt");
    CHECK(!expected.empty()); // golden file must exist and be non-empty

    if (out.str() != expected) {
        // Surface the first differing line to make failures debuggable.
        std::istringstream a(out.str());
        std::istringstream b(expected);
        std::string la_line, lb_line;
        int n = 1;
        while (std::getline(a, la_line) && std::getline(b, lb_line)) {
            if (la_line != lb_line) {
                throw la::test::Failure{"line " + std::to_string(n) + ": got [" + la_line +
                                        "] want [" + lb_line + "]"};
            }
            ++n;
        }
        throw la::test::Failure{"report length differs from golden (got " +
                                std::to_string(out.str().size()) + " bytes, want " +
                                std::to_string(expected.size()) + ")"};
    }
}

TEST_CASE("integration: analyze JSON report matches the golden file") {
    Options opt;
    opt.command = Command::Analyze;
    opt.input_path = kDataDir + "/valid_small.log";
    opt.report = ReportFormat::Json;

    std::ostringstream out, err;
    const int code = run(opt, out, err);
    CHECK_EQ(code, 0);
    CHECK(err.str().empty());

    const std::string expected = slurp(kDataDir + "/valid_small.expected.json");
    CHECK(!expected.empty());
    if (out.str() != expected) {
        std::istringstream a(out.str());
        std::istringstream b(expected);
        std::string la_line, lb_line;
        int n = 1;
        while (std::getline(a, la_line) && std::getline(b, lb_line)) {
            if (la_line != lb_line) {
                throw la::test::Failure{"json line " + std::to_string(n) + ": got [" +
                                        la_line + "] want [" + lb_line + "]"};
            }
            ++n;
        }
        throw la::test::Failure{"json length differs from golden"};
    }
}

TEST_CASE("integration: --strict exits 3 when malformed lines are present") {
    Options opt;
    opt.command = Command::Analyze;
    opt.input_path = kDataDir + "/valid_small.log"; // contains one bad line
    opt.strict = true;
    std::ostringstream out, err;
    CHECK_EQ(run(opt, out, err), 3);
    CHECK(!out.str().empty());   // the report is still rendered
    CHECK(!err.str().empty());   // and a diagnostic is printed
}

TEST_CASE("integration: missing file exits 2") {
    Options opt;
    opt.command = Command::Analyze;
    opt.input_path = kDataDir + "/does_not_exist.log";
    std::ostringstream out, err;
    CHECK_EQ(run(opt, out, err), 2);
    CHECK(!err.str().empty());
}
