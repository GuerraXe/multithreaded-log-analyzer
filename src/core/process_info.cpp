#include "core/process_info.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h> // PROCESS_MEMORY_COUNTERS; K32GetProcessMemoryInfo lives in kernel32
#endif

namespace la {

std::optional<std::uint64_t> peak_working_set_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (::K32GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<std::uint64_t>(pmc.PeakWorkingSetSize);
    }
    return std::nullopt;
#else
    return std::nullopt;
#endif
}

} // namespace la
