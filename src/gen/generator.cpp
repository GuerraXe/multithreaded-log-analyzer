#include "gen/generator.hpp"

#include "parse/timestamp.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ostream>
#include <random>
#include <string>

namespace la {
namespace {

// Portable helpers on top of std::mt19937_64 (whose sequence is standardised).
// Avoids std::normal/lognormal_distribution, whose output is implementation
// defined, so `generate_log` is byte-identical across standard libraries.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : engine_(seed) {}

    double uniform() { // [0, 1)
        return static_cast<double>(engine_() >> 11) * (1.0 / 9007199254740992.0);
    }
    std::uint64_t below(std::uint64_t n) { return engine_() % n; }

    double normal() {
        // Box-Muller; cache the second variate.
        if (have_spare_) {
            have_spare_ = false;
            return spare_;
        }
        double u1 = uniform();
        const double u2 = uniform();
        if (u1 < 1e-300) u1 = 1e-300;
        const double mag = std::sqrt(-2.0 * std::log(u1));
        spare_ = mag * std::sin(2.0 * 3.14159265358979323846 * u2);
        have_spare_ = true;
        return mag * std::cos(2.0 * 3.14159265358979323846 * u2);
    }

private:
    std::mt19937_64 engine_;
    bool have_spare_ = false;
    double spare_ = 0.0;
};

struct Endpoint {
    const char* method;
    const char* path;
};

constexpr std::array<Endpoint, 12> kEndpoints = {{
    {"GET", "/v1/users"},        {"GET", "/v1/users/42"},
    {"GET", "/v1/orders"},       {"POST", "/v1/checkout"},
    {"GET", "/v1/search"},       {"GET", "/health"},
    {"POST", "/v1/login"},       {"PUT", "/v1/users/42"},
    {"DELETE", "/v1/sessions/abc"}, {"GET", "/v1/products"},
    {"GET", "/v1/cart"},         {"POST", "/v1/events"},
}};

constexpr std::array<const char*, 6> kServices = {
    "api-gateway", "order-service", "auth-service",
    "catalog-service", "search-service", "scheduler",
};

const char* pick_service(Rng& rng) {
    // api-gateway heavy, scheduler light.
    const double u = rng.uniform();
    if (u < 0.40) return kServices[0];
    if (u < 0.60) return kServices[1];
    if (u < 0.75) return kServices[2];
    if (u < 0.88) return kServices[3];
    if (u < 0.97) return kServices[4];
    return kServices[5];
}

const char* pick_level(Rng& rng) {
    const double u = rng.uniform();
    if (u < 0.85) return "INFO";
    if (u < 0.95) return "WARN";
    if (u < 0.99) return "ERROR";
    return "FATAL";
}

std::size_t pick_endpoint_zipf(Rng& rng) {
    // Weight of rank k (1-based) is 1/k; draw against the cumulative sum.
    static double cumulative[kEndpoints.size()];
    static bool ready = false;
    if (!ready) {
        double acc = 0.0;
        for (std::size_t k = 0; k < kEndpoints.size(); ++k) {
            acc += 1.0 / static_cast<double>(k + 1);
            cumulative[k] = acc;
        }
        for (double& c : cumulative) c /= acc;
        ready = true;
    }
    const double u = rng.uniform();
    for (std::size_t k = 0; k < kEndpoints.size(); ++k) {
        if (u <= cumulative[k]) return k;
    }
    return kEndpoints.size() - 1;
}

int pick_status(Rng& rng, bool severe) {
    if (severe) {
        static const int s5[] = {500, 502, 503};
        return s5[rng.below(3)];
    }
    const double u = rng.uniform();
    if (u < 0.85) {
        static const int s2[] = {200, 200, 200, 201, 204};
        return s2[rng.below(5)];
    }
    if (u < 0.93) {
        static const int s3[] = {301, 304};
        return s3[rng.below(2)];
    }
    if (u < 0.99) {
        static const int s4[] = {400, 401, 403, 404, 429};
        return s4[rng.below(5)];
    }
    static const int s5[] = {500, 503};
    return s5[rng.below(2)];
}

const char* pick_message(Rng& rng, const char* level) {
    if (level[0] == 'E') { // ERROR
        static const char* m[] = {"upstream timeout", "payment declined",
                                  "dependency unavailable", "database connection lost"};
        return m[rng.below(4)];
    }
    if (level[0] == 'F') { // FATAL
        static const char* m[] = {"worker pool exhausted", "out of memory"};
        return m[rng.below(2)];
    }
    if (level[0] == 'W') { // WARN
        static const char* m[] = {"slow downstream", "retrying request", "cache miss",
                                  "high queue depth"};
        return m[rng.below(4)];
    }
    static const char* m[] = {"request completed", "ok", "cache hit", "job finished"};
    return m[rng.below(4)];
}

} // namespace

void generate_log(std::ostream& os, const GenOptions& opt) {
    Rng rng(opt.seed);
    std::int64_t ts = 1'767'225'600'000; // 2026-01-01T00:00:00Z

    std::string line;
    line.reserve(160);
    char numbuf[32];

    for (std::uint64_t i = 0; i < opt.lines; ++i) {
        // Monotonic non-decreasing timestamp; mean gap ~ 8 ms.
        double u = rng.uniform();
        if (u < 1e-300) u = 1e-300;
        ts += static_cast<std::int64_t>(-8.0 * std::log(u));

        const char* level = pick_level(rng);
        const char* service = pick_service(rng);
        const bool severe = (level[0] == 'E' || level[0] == 'F');
        const bool http = rng.uniform() < 0.70;

        line.clear();
        line += format_timestamp(ts);
        line += " | ";
        line += level;
        line += " | ";
        line += service;
        line += " | ";

        if (http) {
            const Endpoint& e = kEndpoints[pick_endpoint_zipf(rng)];
            line += e.method;
            line += ' ';
            line += e.path;
            line += " | ";
            std::snprintf(numbuf, sizeof numbuf, "%d", pick_status(rng, severe));
            line += numbuf;
            line += " | ";
            double ms = std::exp(3.0 + 0.9 * rng.normal()); // log-normal, median ~20 ms
            if (ms < 0.05) ms = 0.05;
            if (ms > 60'000.0) ms = 60'000.0;
            std::snprintf(numbuf, sizeof numbuf, "%.3f", ms);
            line += numbuf;
            line += " | ";
        } else {
            line += " |  |  | "; // empty request / status / duration
        }

        line += pick_message(rng, level);
        line += '\n';

        os.write(line.data(), static_cast<std::streamsize>(line.size()));
    }
}

} // namespace la
