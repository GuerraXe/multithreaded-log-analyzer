#include "concurrency/parallel_aggregate.hpp"

#include "io/chunking.hpp"

#include <algorithm>
#include <cstdint>
#include <thread>
#include <vector>

namespace la {

unsigned resolve_thread_count(unsigned requested) {
    // Upper bound on workers. A pathological --threads / --threads-list value
    // (e.g. 100000) would otherwise try to spawn that many std::jthreads; the
    // std::system_error thrown when the OS refuses is unhandled and aborts the
    // process. The ceiling sits far above any real core count and above the
    // 1..64 range the equivalence suite pins, so results are unaffected.
    constexpr unsigned kMaxThreads = 1024;
    if (requested != 0) return requested < kMaxThreads ? requested : kMaxThreads;
    const unsigned hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1u : hw;
}

Aggregate parallel_aggregate(std::string_view buffer, const ILogFormat& fmt,
                             const RecordFilter& filter, const AggregateOptions& opt,
                             unsigned threads) {
    const unsigned n = resolve_thread_count(threads);
    if (n <= 1) {
        return aggregate_buffer(buffer, fmt, filter, opt);
    }

    const std::vector<std::string_view> chunks = split_into_chunks(buffer, n);
    if (chunks.size() <= 1) {
        return aggregate_buffer(buffer, fmt, filter, opt);
    }

    // Per-chunk starting line number: chunk 0 begins at opt.line_number_base;
    // each subsequent chunk begins that many newlines later. Chunks split on
    // newline boundaries, so newline count == line count for every chunk but
    // possibly the last (which has no chunk after it).
    std::vector<std::uint64_t> line_base(chunks.size());
    {
        std::uint64_t acc = opt.line_number_base;
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            line_base[i] = acc;
            acc += static_cast<std::uint64_t>(
                std::count(chunks[i].begin(), chunks[i].end(), '\n'));
        }
    }

    // Reserve so element addresses are stable while workers write into them.
    std::vector<Aggregate> partials;
    partials.reserve(chunks.size());
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        partials.emplace_back(opt.interval_ms, opt.malformed_sample_limit,
                              opt.collect_durations);
    }

    {
        std::vector<std::jthread> workers;
        workers.reserve(chunks.size());
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            workers.emplace_back([&, i] {
                AggregateOptions local = opt;
                local.line_number_base = line_base[i];
                partials[i] = aggregate_buffer(chunks[i], fmt, filter, local);
            });
        }
    } // every jthread joins here; no lock was taken on the hot path

    Aggregate result(opt.interval_ms, opt.malformed_sample_limit, opt.collect_durations);
    for (const Aggregate& p : partials) result.merge(p);
    return result;
}

} // namespace la
