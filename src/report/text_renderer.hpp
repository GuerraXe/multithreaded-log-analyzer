#pragma once

#include "stats/report.hpp"

#include <iosfwd>

namespace la {

// Render a Report as a human-readable multi-section summary. Output is a pure
// function of the Report (no timing, no colour, no locale-dependent
// formatting), so it is stable enough to diff against a golden file.
void render_text(const Report& r, std::ostream& os);

// "30s" / "5m" / "1h" / "90s" label for an interval in milliseconds.
std::string format_interval(std::int64_t interval_ms);

} // namespace la
