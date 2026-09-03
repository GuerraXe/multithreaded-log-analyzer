#pragma once

#include "parse/log_format.hpp"

#include <memory>
#include <string_view>

namespace la {

// Construct a log format by name. An empty name selects the default ("pipe").
// Returns nullptr for an unknown name.
std::unique_ptr<ILogFormat> make_log_format(std::string_view name);

} // namespace la
