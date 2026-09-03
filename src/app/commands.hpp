#pragma once

#include "cli/options.hpp"

#include <iosfwd>

namespace la {

// `analyze` lives in app/analyze.hpp; these are the other two verbs.

// Write a synthetic dataset to opt.input_path. Returns 0, or 2 on write error.
int run_gen(const Options& opt, std::ostream& out, std::ostream& err);

// Sweep thread counts over opt.input_path and print a throughput comparison.
// Returns 0, or 2 on read error.
int run_benchmark(const Options& opt, std::ostream& out, std::ostream& err);

} // namespace la
