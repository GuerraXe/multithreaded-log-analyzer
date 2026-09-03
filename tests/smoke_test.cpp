#include "support/test_framework.hpp"

#include "app/version.hpp"

#include <string>

TEST_CASE("version_string reports the project name") {
    const std::string v = la::version_string();
    CHECK(v.find("loganalyzer") != std::string::npos);
}

TEST_CASE("version_string reports the version constant") {
    const std::string v = la::version_string();
    CHECK(v.find(std::string(la::kVersion)) != std::string::npos);
}

TEST_CASE("test framework arithmetic sanity") {
    CHECK_EQ(2 + 2, 4);
}
