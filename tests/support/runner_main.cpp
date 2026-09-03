#include "support/test_framework.hpp"

// Single entry point for the whole test binary. Every TEST_CASE across the
// suite self-registers; this just runs them and returns the pass/fail code.
int main() {
    return la::test::run_all();
}
