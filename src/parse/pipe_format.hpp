#pragma once

#include "parse/log_format.hpp"

namespace la {

// The v1 log grammar: seven "<SP>|<SP>"-separated fields per line,
//   timestamp | level | service | request | status | duration_ms | message
// See SPEC.md section 3 for the full field rules and malformed-line criteria.
class PipeDelimitedFormat final : public ILogFormat {
public:
    std::string_view name() const override { return "pipe"; }
    ParseResult parse_line(std::string_view line) const override;
};

} // namespace la
