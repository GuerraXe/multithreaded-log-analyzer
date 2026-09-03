#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace la {

// Split `buffer` into at most `n` contiguous, non-overlapping pieces whose
// concatenation is exactly `buffer`. Every split point is moved forward to the
// byte just after a '\n', so:
//   * every line lies wholly within one chunk (no line straddles a boundary);
//   * every chunk except possibly the last ends with '\n';
//   * no returned chunk is empty.
//
// Fewer than `n` chunks are returned when the buffer is small or has few
// newlines (e.g. one line longer than a nominal chunk yields one chunk). An
// empty buffer yields an empty vector. `n == 0` is treated as `n == 1`.
std::vector<std::string_view> split_into_chunks(std::string_view buffer, std::size_t n);

} // namespace la
