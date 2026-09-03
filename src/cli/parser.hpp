#pragma once

#include "cli/options.hpp"

#include <string>

namespace la {

struct ArgParse {
    bool ok = false;
    Options options;
    std::string error; // populated when ok == false
};

// Parse a full argv (including argv[0]) into Options. Never throws. On failure
// `ok` is false and `error` holds a one-line, user-facing message.
ArgParse parse_args(int argc, char** argv);

} // namespace la
