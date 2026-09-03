#pragma once

#include <cstdint>
#include <iosfwd>

namespace la {

struct GenOptions {
    std::uint64_t lines = 100'000;
    std::uint64_t seed = 1;
};

// Write `opt.lines` synthetic log lines (format `pipe`) to `os`. Fully
// deterministic: the same seed and line count produce byte-identical output on
// every platform. The mix approximates a busy web service:
//   * ~85% INFO, ~10% WARN, ~4% ERROR, ~1% FATAL
//   * ~70% of lines carry an HTTP request; endpoint popularity is Zipfian
//   * latencies are log-normal (median ~20 ms, a long tail into seconds)
//   * timestamps increase monotonically
// The output contains no malformed or blank lines.
void generate_log(std::ostream& os, const GenOptions& opt);

} // namespace la
