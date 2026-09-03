#pragma once

#include "stats/report.hpp"

#include <iosfwd>
#include <string>
#include <string_view>

namespace la {

// Render a Report as a single pretty-printed JSON object (2-space indent, LF
// newlines, ASCII). The schema is frozen at M4; see
// tests/data/valid_small.expected.json. Latency-ms fields are `null` when
// there are no timed samples, and (in histogram mode) also when the
// percentile exceeds the 10s histogram ceiling.
void render_json(const Report& r, std::ostream& os);

// JSON-escape and quote a string. Exposed for testing.
std::string json_quote(std::string_view s);

} // namespace la
