#include "support/test_framework.hpp"

#include "core/process_info.hpp"

#include <cstdint>
#include <vector>

using namespace la;

TEST_CASE("process_info: peak working set is reported on this platform") {
#ifdef _WIN32
    // Touch some memory so the peak is meaningfully non-zero.
    std::vector<char> scratch(4 * 1024 * 1024, 1);
    volatile char sink = scratch[scratch.size() - 1];
    (void)sink;

    const auto v = peak_working_set_bytes();
    CHECK(v.has_value());
    CHECK(*v > std::uint64_t{0});
#else
    CHECK(!peak_working_set_bytes().has_value());
#endif
}
