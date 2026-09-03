# Multithreaded Log Analyzer (`loganalyzer`)

A command-line tool that processes large application / server log files and
extracts operational statistics — error and warning counts, the most frequent
errors, slow endpoints, HTTP status-code mix, traffic over time, and failures
per service — with selectable **sequential** and **multithreaded** execution so
their performance can be measured against each other on real data.

The multithreaded path is proven to produce results **identical** to the
sequential path (see [Correctness](#correctness-multithreaded--sequential)).

Status: **v1.0** — feature-complete against [SPEC.md](SPEC.md). 133 tests,
warning-clean at `/W4 /permissive-`.

---

## Build

Requires CMake ≥ 3.21 and a C++20 compiler. On Windows with the VS 2022 Build
Tools the bundled CMake and the `windows-msvc` preset need no Developer Prompt:

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug      # or windows-msvc-release for benchmarking
ctest   --preset windows-msvc-debug
```

Binaries: `loganalyzer` (CLI) and `loganalyzer_tests` (one self-registering
test binary, run by CTest).

---

## Usage

```
loganalyzer analyze   <file> [--threads N] [filters] [--report text|json] [-o out]
loganalyzer benchmark <file> [--threads-list 1,2,4,8] [--warmup N] [--repeat N]
loganalyzer gen       <file> [--lines N] [--seed S]
loganalyzer version | help [command]
```

**Filters** (all AND together; repeated values within one OR):
`--from` / `--to` (ISO-8601, half-open `[from, to)`), `--level` /
`--level-only`, `--service` (repeatable), `--status-class Nxx` (repeatable),
`--path-prefix`, `--path-contains`, `--top N`, `--interval 30s|1m|5m|1h`,
`--exact-percentiles`.

**Exit codes:** `0` ok · `1` usage error · `2` I/O error · `3` `--strict` and
malformed lines present.

### Example

```
$ loganalyzer analyze server.log --threads 8 --level warn --top 5

busiest endpoints
         3  GET /v1/users/42  mean=7.9ms p50=10 p90=20 p99=20 max=12
         3  POST /v1/checkout  mean=455.7ms p50=100 p90=2000 p99=2000 max=1201
...
traffic (1m buckets)
  2026-03-01T09:00:00Z  req=7 err=3
  2026-03-01T09:01:00Z  req=3 err=1
```

`--report json` emits a single object with a frozen schema — see
[docs/JSON_SCHEMA.md](docs/JSON_SCHEMA.md).

---

## Log format

v1 supports one format, `pipe`: seven `" | "`-separated fields per line —

```
timestamp | level | service | request | status | duration_ms | message
```

Full grammar and malformed-line rules are in [SPEC.md §3](SPEC.md). The parser
sits behind an `ILogFormat` interface so other formats can be added later
without touching filtering, aggregation, or reporting.

---

## Why multithreading, and how it is done

**Why.** Parsing and aggregating a large log is embarrassingly parallel over
disjoint line ranges: each line is independent, and the per-line work
(tokenise, validate, hash-map updates) is CPU-bound. On a many-core machine a
single thread leaves most of the throughput on the table — measured below at
**~10×** on a 24-core laptop.

**How work is divided.** The file is memory-mapped once
([`io/mapped_file`](src/io/mapped_file.hpp)) and split into one contiguous
byte range per worker
([`io/chunking`](src/io/chunking.hpp)), with every split point moved forward
to the next newline so each line belongs to exactly one worker. Large
sequential ranges are friendly to the hardware prefetcher and keep each worker
on its own cache lines.

**Synchronization strategy.** *None on the hot path* — no mutex, no atomics,
no shared containers during parse / filter / aggregate. Each worker
(`std::jthread`, one per chunk) fills a private `Aggregate`. The only
synchronization is the thread join; the main thread then folds the partials
with an **associative, commutative** `merge`
([`concurrency/parallel_aggregate`](src/concurrency/parallel_aggregate.hpp)).

**Performance tradeoffs.** Thread creation and the merge fold are fixed
overhead; for small inputs they exceed the work and the sequential path wins
(demonstrated below). Throughput also stops scaling once memory bandwidth
saturates, and per-thread efficiency drops past the physical performance-core
count on a hybrid CPU. `benchmark` is built to show exactly where these
crossovers are and prints a plain verdict rather than assuming parallel is
faster.

### Correctness: multithreaded ≡ sequential

For the same input and options, the multithreaded result is **identical** to
the sequential one: all counts, sums, min/max, histogram bins, time buckets,
top-N lists, and the **byte-for-byte rendered JSON** match. Latency is summed
in integer microseconds so the totals do not depend on how lines were
partitioned; ranked lists use a total order (value desc, then key asc) so they
do not depend on hash-map iteration order or thread count. The one value
allowed to differ is per-endpoint standard deviation (`double` summation
order), bounded to `1e-9` relative.

This is enforced by
[`tests/concurrency/equivalence_tests.cpp`](tests/concurrency/equivalence_tests.cpp),
which checks a fixed-seed 4000-line log for threads ∈ {1,2,3,4,7,8,16,32,64},
with filters, with more threads than lines, and on empty input.

---

## Measured performance

Release `/O2`, warm page cache, 432 MB generated dataset
(`gen --lines 5000000 --seed 1`), on a 13th-gen Core i9-13980HX
(8 P-cores + 16 E-cores, 32 threads):

| threads | median ms | records/s | MB/s | speedup | efficiency |
|---:|---:|---:|---:|---:|---:|
| 1 | 2774 | 1.8 M | 156 | 1.00× | 100 % |
| 4 | 835 | 6.0 M | 518 | 3.32× | 83 % |
| 8 | 487 | 10.3 M | 889 | 5.70× | 71 % |
| 16 | 275 | 18.2 M | 1571 | 10.1× | 63 % |
| 32 | 267 | 18.7 M | 1622 | 10.4× | 32 % |

Near-linear to ~4 threads, tapering past the 8 performance cores, then a
plateau near **1.6 GB/s** once memory bandwidth is the limit. A 130 KB input
goes the other way — 8+ threads are *slower* than one. Full methodology,
the thermal-throttling caveat, the small-input numbers, and a worked
bottleneck investigation are in
[docs/PERFORMANCE.md](docs/PERFORMANCE.md).

---

## Project layout

```
src/
  core/        version string, peak-working-set query
  io/          memory-mapped file, newline-aligned chunk splitter
  parse/       ISO-8601 timestamps, LogRecord, ILogFormat, PipeDelimitedFormat
  filter/      RecordFilter (compiled predicate)
  aggregate/   LatencyHistogram, EndpointStat, TimeBuckets, Aggregate + driver
  concurrency/ parallel_aggregate (map / reduce)
  stats/       build_report -> render-ready Report model
  report/      text and JSON renderers
  gen/         deterministic synthetic-dataset generator
  bench/       benchmark harness (timing, speedup, verdict)
  cli/         argv -> Options
  app/         command dispatch, the analyze / benchmark / gen drivers
tests/         one binary per module dir + integration + the equivalence gate
docs/          PERFORMANCE.md, JSON_SCHEMA.md
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for module boundaries, the data-flow
diagram, and the concurrency design in detail.

---

## Limitations (v1)

- One log format (`pipe`); no regex / logfmt / JSON / syslog yet (the
  interface allows adding them).
- No live tail / follow mode, no multi-file input, no compressed input.
- Latency percentiles are histogram-based (bucket upper bounds) unless
  `--exact-percentiles`, which forces single-threaded.
- Timestamps are assumed UTC (`Z`); other zone suffixes are treated as
  malformed.
- A literal `" | "` inside a message field will split that message.
- The `timeline` array is uncapped: a very wide time range with a small
  `--interval` produces many buckets.
- Benchmarks were taken on a laptop CPU that thermally throttles under
  sustained load; medians drift upward through a sweep (see PERFORMANCE.md).

## License

TBD.
