#include "app/version.hpp"

namespace la {

std::string version_string() {
    std::string s;
    s.reserve(kProjectName.size() + 1 + kVersion.size());
    s.append(kProjectName);
    s.push_back(' ');
    s.append(kVersion);
    return s;
}

} // namespace la
