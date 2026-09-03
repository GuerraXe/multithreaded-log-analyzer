#pragma once

#include <string>
#include <string_view>

namespace la {

inline constexpr std::string_view kProjectName = "loganalyzer";
inline constexpr std::string_view kVersion = "1.0.0";

// Human-readable "loganalyzer X.Y.Z" identifier used by the `version`
// command and by report headers.
std::string version_string();

} // namespace la
