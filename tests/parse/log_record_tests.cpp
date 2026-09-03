#include "support/test_framework.hpp"

#include "parse/log_format.hpp"
#include "parse/log_record.hpp"

#include <string>

using namespace la;

TEST_CASE("parse_level: case-insensitive, all names") {
    CHECK(parse_level("TRACE") == Level::Trace);
    CHECK(parse_level("debug") == Level::Debug);
    CHECK(parse_level("InFo") == Level::Info);
    CHECK(parse_level("WARN") == Level::Warn);
    CHECK(parse_level("error") == Level::Error);
    CHECK(parse_level("FaTaL") == Level::Fatal);
}

TEST_CASE("parse_level: unknown names rejected") {
    CHECK(!parse_level("NOTICE").has_value());
    CHECK(!parse_level("").has_value());
    CHECK(!parse_level("INFORMATION").has_value());
    CHECK(!parse_level("INF").has_value());
}

TEST_CASE("to_string(Level) is the canonical upper-case name") {
    CHECK_EQ(std::string(to_string(Level::Trace)), std::string("TRACE"));
    CHECK_EQ(std::string(to_string(Level::Info)), std::string("INFO"));
    CHECK_EQ(std::string(to_string(Level::Fatal)), std::string("FATAL"));
}

TEST_CASE("Level ordering matches severity") {
    CHECK(level_index(Level::Trace) < level_index(Level::Info));
    CHECK(level_index(Level::Info) < level_index(Level::Error));
    CHECK(level_index(Level::Error) < level_index(Level::Fatal));
    CHECK_EQ(level_index(Level::Fatal), 5);
}

TEST_CASE("parse_method: exact upper-case only") {
    CHECK(parse_method("GET") == Method::Get);
    CHECK(parse_method("POST") == Method::Post);
    CHECK(parse_method("OPTIONS") == Method::Options);
    CHECK(!parse_method("get").has_value());
    CHECK(!parse_method("").has_value());
    CHECK(!parse_method("FETCH").has_value());
}

TEST_CASE("to_string(Method): None is empty, others canonical") {
    CHECK_EQ(std::string(to_string(Method::None)), std::string(""));
    CHECK_EQ(std::string(to_string(Method::Delete)), std::string("DELETE"));
    CHECK_EQ(std::string(to_string(Method::Patch)), std::string("PATCH"));
}

TEST_CASE("reason_code slugs are stable") {
    CHECK_EQ(std::string(reason_code(ParseError::None)), std::string("none"));
    CHECK_EQ(std::string(reason_code(ParseError::FieldCount)), std::string("field_count"));
    CHECK_EQ(std::string(reason_code(ParseError::Timestamp)), std::string("timestamp"));
    CHECK_EQ(std::string(reason_code(ParseError::Level)), std::string("level"));
    CHECK_EQ(std::string(reason_code(ParseError::Service)), std::string("service"));
    CHECK_EQ(std::string(reason_code(ParseError::Request)), std::string("request"));
    CHECK_EQ(std::string(reason_code(ParseError::Status)), std::string("status"));
    CHECK_EQ(std::string(reason_code(ParseError::Duration)), std::string("duration"));
}

TEST_CASE("is_blank_line: empty and whitespace-only") {
    CHECK(is_blank_line(""));
    CHECK(is_blank_line("   "));
    CHECK(is_blank_line("\t \r\n"));
    CHECK(!is_blank_line("x"));
    CHECK(!is_blank_line("  x  "));
}
