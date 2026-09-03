#include "parse/formats.hpp"

#include "parse/pipe_format.hpp"

namespace la {

std::unique_ptr<ILogFormat> make_log_format(std::string_view name) {
    if (name.empty() || name == "pipe") {
        return std::make_unique<PipeDelimitedFormat>();
    }
    return nullptr;
}

} // namespace la
