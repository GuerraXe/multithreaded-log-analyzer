#pragma once

#include <cstdint>
#include <optional>

namespace la {

// Peak working-set size of the current process in bytes, if the platform can
// report it (Windows: K32GetProcessMemoryInfo). std::nullopt otherwise.
std::optional<std::uint64_t> peak_working_set_bytes();

} // namespace la
