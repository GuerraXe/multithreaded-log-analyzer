#pragma once

// Minimal, dependency-free test framework. Deliberately hand-rolled instead
// of vendoring a third-party framework (Catch2/doctest/GoogleTest) per the
// project's "no unnecessary dependencies" constraint -- the surface area we
// need (register a named test, assert a condition, report pass/fail) is
// small enough that owning it is simpler than depending on it.
//
// Usage:
//   TEST_CASE("thing does X") {
//       CHECK(1 + 1 == 2);
//       CHECK_EQ(compute(), 42);
//   }
// All TEST_CASE blocks in a binary self-register; a single main() (see
// runner_main.cpp) runs them all and reports a summary.

#include <cstddef>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace la::test {

struct Failure {
    std::string message;
};

inline std::vector<std::pair<std::string, std::function<void()>>>& registry() {
    static std::vector<std::pair<std::string, std::function<void()>>> r;
    return r;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().emplace_back(std::move(name), std::move(fn));
    }
};

inline int run_all() {
    int failed = 0;
    for (auto& [name, fn] : registry()) {
        try {
            fn();
            std::cout << "[ OK ] " << name << "\n";
        } catch (const Failure& f) {
            std::cout << "[FAIL] " << name << ": " << f.message << "\n";
            ++failed;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << name << ": unexpected exception: " << e.what() << "\n";
            ++failed;
        }
    }
    const auto total = registry().size();
    std::cout << (failed == 0 ? "All tests passed" : "SOME TESTS FAILED")
              << " (" << (total - static_cast<size_t>(failed)) << "/" << total << ")\n";
    return failed == 0 ? 0 : 1;
}

} // namespace la::test

#define LA_TEST_CONCAT_INNER(a, b) a##b
#define LA_TEST_CONCAT(a, b) LA_TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                            \
    static void LA_TEST_CONCAT(la_test_fn_, __LINE__)();                           \
    static ::la::test::Registrar LA_TEST_CONCAT(la_test_reg_, __LINE__){           \
        (name), LA_TEST_CONCAT(la_test_fn_, __LINE__)};                            \
    static void LA_TEST_CONCAT(la_test_fn_, __LINE__)()

#define CHECK(cond)                                                                \
    do {                                                                          \
        if (!(cond)) {                                                            \
            std::ostringstream oss;                                               \
            oss << "CHECK failed: " #cond << " (" << __FILE__ << ":" << __LINE__  \
                << ")";                                                           \
            throw ::la::test::Failure{oss.str()};                                 \
        }                                                                         \
    } while (0)

#define CHECK_EQ(a, b)                                                            \
    do {                                                                          \
        auto&& la_check_a = (a);                                                  \
        auto&& la_check_b = (b);                                                  \
        if (!(la_check_a == la_check_b)) {                                        \
            std::ostringstream oss;                                               \
            oss << "CHECK_EQ failed: " #a " == " #b " (" << la_check_a << " != "  \
                << la_check_b << ") at " << __FILE__ << ":" << __LINE__;          \
            throw ::la::test::Failure{oss.str()};                                 \
        }                                                                         \
    } while (0)

#define CHECK_THROWS(expr)                                                        \
    do {                                                                          \
        bool la_threw = false;                                                    \
        try {                                                                     \
            (void)(expr);                                                         \
        } catch (...) {                                                           \
            la_threw = true;                                                      \
        }                                                                         \
        if (!la_threw) {                                                          \
            std::ostringstream oss;                                               \
            oss << "CHECK_THROWS failed: " #expr " did not throw (" << __FILE__   \
                << ":" << __LINE__ << ")";                                        \
            throw ::la::test::Failure{oss.str()};                                 \
        }                                                                         \
    } while (0)
