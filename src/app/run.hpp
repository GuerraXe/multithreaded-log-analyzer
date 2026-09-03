#pragma once

#include "cli/options.hpp"

#include <iosfwd>

namespace la {

// Execute a parsed command. Returns the process exit code. All output goes to
// `out`; diagnostics to `err`.
int run(const Options& opt, std::ostream& out, std::ostream& err);

} // namespace la
