#include "support/test_framework.hpp"

#include "io/mapped_file.hpp"

#include <fstream>
#include <sstream>
#include <string>

using namespace la;

namespace {
#ifndef LA_DATA_DIR
#define LA_DATA_DIR "."
#endif
const std::string kDataDir = LA_DATA_DIR;

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
} // namespace

TEST_CASE("mapped_file: maps an existing file and exposes its exact bytes") {
    const std::string path = kDataDir + "/valid_small.log";
    const MappedFile f = MappedFile::open(path);
    CHECK(f.ok());
    CHECK(f.error().empty());

    const std::string expected = slurp(path);
    CHECK_EQ(f.size(), expected.size());
    CHECK_EQ(std::string(f.data()), expected);
}

TEST_CASE("mapped_file: empty file is ok with a zero-length view") {
    const MappedFile f = MappedFile::open(kDataDir + "/empty.log");
    CHECK(f.ok());
    CHECK_EQ(f.size(), std::size_t{0});
    CHECK(f.data().empty());
}

TEST_CASE("mapped_file: missing file reports an error and is not ok") {
    const MappedFile f = MappedFile::open(kDataDir + "/nope_does_not_exist.log");
    CHECK(!f.ok());
    CHECK(!static_cast<bool>(f));
    CHECK(!f.error().empty());
}

TEST_CASE("mapped_file: CRLF bytes are preserved verbatim") {
    const std::string path = kDataDir + "/crlf.log";
    const MappedFile f = MappedFile::open(path);
    CHECK(f.ok());
    CHECK_EQ(std::string(f.data()), slurp(path));
    CHECK(f.data().find('\r') != std::string_view::npos);
}

TEST_CASE("mapped_file: move keeps the view valid") {
    MappedFile a = MappedFile::open(kDataDir + "/valid_small.log");
    const std::size_t n = a.size();
    const std::string first10(a.data().substr(0, 10));

    MappedFile b = std::move(a);
    CHECK(b.ok());
    CHECK_EQ(b.size(), n);
    CHECK_EQ(std::string(b.data().substr(0, 10)), first10);
}
