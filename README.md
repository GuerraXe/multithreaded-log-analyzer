# Multithreaded Log Analyzer (`loganalyzer`)

A command-line tool that processes large application / server log files and
extracts operational statistics — error counts, most frequent errors, slow
endpoints, status-code mix, traffic over time, and failures per service — with
selectable **sequential** and **multithreaded** execution so their performance
can be measured against each other on real data.

> Status: in development. Milestone **M0** (repo skeleton) complete.
> See [SPEC.md](SPEC.md) for the full contract and [ARCHITECTURE.md](ARCHITECTURE.md)
> for module boundaries and the concurrency design.

## Build

Requires CMake ≥ 3.21 and a C++20 compiler. On Windows with the VS 2022 Build
Tools, the bundled CMake and the `windows-msvc` preset need no Developer Prompt:

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

The CLI binary is `loganalyzer`; the test binary is `loganalyzer_tests`.

## Usage (target interface)

```
loganalyzer analyze  <file> [--threads N] [filters] [--report text|json]
loganalyzer benchmark <file> [--threads-list 1,2,4,8] [--repeat N]
loganalyzer gen      <file> [--lines N] [--seed S]
loganalyzer version | help [command]
```

Filters: `--from` / `--to` (ISO-8601, half-open), `--level` / `--level-only`,
`--service` (repeatable), `--status-class Nxx` (repeatable), `--path-prefix`,
`--path-contains`, `--top N`, `--interval 30s|1m|5m|1h`.

## Log format

v1 supports one format, `pipe`: seven `" | "`-separated fields per line —
`timestamp | level | service | request | status | duration_ms | message`.
Full grammar and malformed-line rules are in [SPEC.md §3](SPEC.md). The parser
sits behind an `ILogFormat` interface so other formats can be added later
without touching aggregation.

## Why multithreading, and how it is done

*(Expanded with measured numbers at milestone M8. Summary of the design:)*

- **Why.** Parsing and aggregating a large log is embarrassingly parallel over
  disjoint line ranges: each line is independent, and the per-line work
  (tokenize, validate, hash-map updates) is CPU-bound. On a many-core machine
  this is throughput left on the table if run single-threaded.
- **Work division.** The file is memory-mapped once and split into one
  contiguous byte range per thread, with each split point moved forward to the
  next newline so every line belongs to exactly one worker.
- **Synchronization strategy.** None on the hot path — no locks, no atomics, no
  shared containers during parse / filter / aggregate. Each worker fills its own
  `PartialAggregate`. The only synchronization is the thread join; the main
  thread then folds the partials with an associative, commutative `merge`.
- **Performance tradeoffs.** Thread creation and the merge step are fixed
  overhead; for small inputs they exceed the work and the sequential path wins.
  Throughput also stops scaling once memory bandwidth saturates, and
  per-thread efficiency drops past the physical P-core count on a hybrid CPU.
  The `benchmark` command is built to show exactly where these crossovers are,
  and prints a plain verdict rather than assuming parallel is faster.
- **Correctness.** The multithreaded result is identical to the sequential one:
  all counts, sums, min/max, histogram bins, and top-N lists are bitwise
  identical (durations are accumulated as integer microseconds to make the sums
  order-independent); only the standard-deviation figure is allowed to differ,
  within a `1e-6` relative tolerance. This equivalence is enforced by the test
  suite across 1–64 threads.

## Benchmark methodology

Summarized in [SPEC.md §7](SPEC.md); full methodology and measured results in
[docs/PERFORMANCE.md](docs/PERFORMANCE.md). In short: fixed-seed generated
datasets at several sizes, warmup run discarded, median of repeated runs,
phase time measured separately from file I/O and report rendering, cold-cache
and warm-cache reported separately, results captured only after all correctness
gates pass.

## Limitations (v1)

- One log format (`pipe`); no regex / logfmt / JSON / syslog yet.
- No live tail / follow mode, no multi-file input, no compressed input.
- Latency percentiles are histogram-based (reported as bucket upper bounds)
  unless `--exact-percentiles` is given, which forces single-threaded.
- Timestamps are assumed UTC (`Z`); other zone suffixes are treated as
  malformed.
- A literal `" | "` inside a message field will split that message.

## License

TBD.
