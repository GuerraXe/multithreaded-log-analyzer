# Multithreaded Log Analyzer — Architecture

This document describes the module boundaries, the data flow, and the
concurrency design. It is kept in sync with the code; when a boundary changes,
this file changes in the same commit.

---

## 1. Module overview

Each module is a static library with a single responsibility. Dependencies
point downward only; nothing depends on `cli` or `app`.

```
            +------+      +-----------+      +--------+
   argv --> | cli  | ---> |    app    | ---> | report | --> stdout / file
            +------+      +-----+-----+      +--------+
                                |                ^
                                v                |
                          +-----------+     +---------+
                          |concurrency| --> |  stats  |
                          +-----+-----+     +---------+
                                |                ^
                 +--------------+--------------+ |
                 v              v              v |
             +------+      +--------+      +-----------+
             |  io  | ---> | parse  | ---> | aggregate |
             +------+      +--------+      +-----------+
                              ^
                              |
                          +--------+
                          | filter |
                          +--------+
```

| Module | Directory | Responsibility | Depends on |
|---|---|---|---|
| `core` | `src/core/` | `version_string`; `peak_working_set_bytes` (`K32GetProcessMemoryInfo`); header-only `for_each_line`. | — |
| `io` | `src/io/` | `MappedFile::open` — Win32 `CreateFileMapping` / `MapViewOfFile` with a buffered-read fallback and a zero-length view for empty files. `split_into_chunks` — N byte ranges aligned forward to `\n`, no empty chunks, exact cover. | — |
| `parse` | `src/parse/` | `LogRecord`; `Level` / `Method` enums; `parse_timestamp` / `format_timestamp`; `ILogFormat` interface; `PipeDelimitedFormat`; `ParseError` reason codes; `make_log_format`. | — |
| `filter` | `src/filter/` | `FilterSpec` → compiled stateless `RecordFilter`. `matches(const LogRecord&) -> bool`. | `parse` |
| `aggregate` | `src/aggregate/` | `LatencyHistogram`, `EndpointStat`, `TimeBuckets`, `Aggregate` + `aggregate_buffer` driver. Transparent-hash `StringMap<V>` so hot-path map probes take a `string_view` and allocate only on first sight. `merge` folds partials associatively. | `parse`, `filter` |
| `concurrency` | `src/concurrency/` | `parallel_aggregate(buffer, format, filter, opts, threads) -> Aggregate`. Map workers over chunks; serial `merge` fold. `resolve_thread_count`. | `io`, `aggregate`, `filter`, `parse` |
| `stats` | `src/stats/` | `build_report` — merged `Aggregate` → render-ready `Report`: top-N with total-order tie-break, histogram or exact percentiles, rates, malformed-sample passthrough. Pure. | `aggregate` |
| `report` | `src/report/` | `render_text` and `render_json` (hand-rolled streaming JSON writer, frozen schema): `Report` → `std::ostream`. | `stats`, `core` |
| `gen` | `src/gen/` | `generate_log` — seed-deterministic synthetic dataset (portable transforms on `std::mt19937_64`). | `parse` |
| `bench` | `src/bench/` | `run_benchmark_core` — warmup + repeat timing of `parallel_aggregate`; median phase time, speedup / efficiency / verdict, peak working set; text + JSON renderers. | `concurrency`, `aggregate`, `core` |
| `cli` | `src/cli/` | `parse_args(argc, argv) -> ArgParse{ok, Options, error}`; validate every flag. | `parse`, `filter` |
| `app` | `src/app/` | `run(const Options&) -> int` dispatch; the `analyze` / `benchmark` / `gen` drivers (file I/O, timing, renderer selection). | all of the above |

`src/main.cpp` is a thin shell: `parse_args`, then `run`.

---

## 2. Data flow

```
file bytes
   │  io::MappedFile
   ▼
mapped buffer  (std::span<const char>, lifetime owns the mapping)
   │  io::split_into_chunks(buffer, n)  — split points moved forward to next '\n'
   ▼
chunk[0..n-1]  (each a std::span<const char> of whole lines)
   │  per chunk, on its own thread:
   │     for each line:
   │        ILogFormat::parse_line(line) ──► LogRecord | ParseError
   │        RecordFilter::matches(record) ? accumulate : skip
   ▼
PartialAggregate[0..n-1]   (no shared state; worker i writes only slot i)
   │  main thread, after join(): fold with Aggregate::merge
   ▼
Aggregate  (merged)
   │  stats::build_report(aggregate, options)
   ▼
Report model  (plain data: tables, histograms, time series, totals)
   │  TextRenderer / JsonRenderer
   ▼
stdout or -o file
```

`LogRecord` holds `std::string_view`s that point into the mapped buffer. They
are valid only while the mapping is alive, which spans the whole aggregation.
`PartialAggregate` copies into owned `std::string` the few things it retains:
service names, endpoint keys (`METHOD SP path`), and error-message strings.

---

## 3. Key types

### `LogRecord` (`parse`)

| Field | Type | Notes |
|---|---|---|
| `epoch_ms` | `int64_t` | UTC milliseconds since the Unix epoch. |
| `level` | `Level` | enum: `Trace Debug Info Warn Error Fatal`. |
| `service` | `std::string_view` | into the mapped buffer. |
| `method` | `Method` | enum incl. `None` for non-HTTP lines. |
| `path` | `std::string_view` | empty when `method == None`. |
| `status` | `uint16_t` | 0 means "no status". |
| `duration_us` | `int64_t` | −1 means "no duration"; otherwise microseconds. |
| `message` | `std::string_view` | into the mapped buffer. |

### `PartialAggregate` (`aggregate`)

- `uint64_t total, kept, malformed, blank; uint64_t bytes;`
- `std::array<uint64_t, 6> by_level;`
- `std::unordered_map<uint16_t, uint64_t> by_status;`
- `std::array<uint64_t, 6> by_status_class;` (index 0 unused)
- `std::unordered_map<std::string, uint64_t> by_service;`
- `std::unordered_map<std::string, uint64_t> error_messages;`
- `std::unordered_map<std::string, EndpointStat> endpoints;`
- `std::unordered_map<std::string, uint64_t> failures_by_service;`
- `std::map<int64_t, BucketCounts> time_buckets;`  (ordered by bucket start)
- `void merge(const PartialAggregate& other);`  — field-wise `+`, map union with `+`.

### `EndpointStat` (`aggregate`)

- `uint64_t count;`               — requests seen (with or without duration)
- `uint64_t timed;`               — requests that had a duration
- `int64_t min_us, max_us;`       — over timed requests
- `int64_t sum_us;`               — exact integer sum, drives mean and CR-1
- `double sum_sq_us;`             — for stddev only (CR-3)
- `LatencyHistogram hist;`        — fixed bucket edges, mergeable

### `LatencyHistogram` (`aggregate`)

Fixed upper edges in milliseconds: `1, 2, 5, 10, 20, 50, 100, 200, 500,
1000, 2000, 5000, 10000, +inf` — 14 buckets. `add(duration_us)` increments one
bucket. `merge` adds bin counts. `percentile(p)` returns the upper edge of the
bucket containing the `p`-th item (reported as an upper bound; the
approximation is documented). `--exact-percentiles` bypasses the histogram by
collecting durations into a vector and sorting (single-threaded only).

---

## 4. Concurrency design

### 4.1 Work division

The whole file is mapped once. `io::split_into_chunks(buffer, n)` produces `n`
contiguous byte ranges. Each split point is moved **forward** to the byte after
the next `\n`, so:

- every byte belongs to exactly one chunk (CR-5);
- every line belongs to exactly one chunk — no line straddles a boundary;
- chunks are large and sequential, which is friendly to the hardware
  prefetcher and keeps each worker on its own cache lines.

If `n` exceeds the number of lines, trailing chunks are empty and their workers
produce an empty `PartialAggregate` (CR-6).

### 4.2 Execution model — map / reduce

```
std::vector<PartialAggregate> partials(n);      // one slot per chunk
std::vector<std::jthread> workers;
for (i in 0..n-1)
    workers.emplace_back([&, i] {
        partials[i] = run_chunk(chunks[i], format, filter);   // pure, no sharing
    });
workers.clear();                                 // joins all (jthread dtor)
Aggregate result;
for (auto& p : partials) result.merge(p);        // main thread, single-threaded fold
```

`run_chunk` parses, filters, and accumulates into a local `PartialAggregate`
that is then moved into `partials[i]`. Worker `i` touches only `partials[i]`
and its own locals.

Default chunk count equals the requested thread count. (If a future change
makes chunk count exceed thread count, a small work-stealing pool replaces the
one-thread-per-chunk loop; the reduce step is unchanged.)

### 4.3 Synchronization strategy

There is **no synchronization on the hot path**: no mutex, no atomics, no
shared containers during parse / filter / aggregate. Workers share only
read-only data (the mapped buffer, the format, the filter). The single
synchronization point is `jthread` join. The reduce (`merge` fold) runs on the
main thread after all joins.

`partials` slots are padded to a cache line to avoid false sharing while
workers write their results.

This is the cheapest correct design for this workload: parsing and aggregation
are embarrassingly parallel over disjoint line ranges, and the merge cost is
`O(distinct keys)`, far smaller than the parse cost for realistic data. If a
profile ever shows merge dominating, the fold becomes a parallel tree-merge;
that does not affect results because `merge` is associative and commutative
(CR-4).

### 4.4 Determinism and equivalence (how the SPEC section 6 guarantees hold)

- **Integer accumulation.** Durations are parsed to integer microseconds. All
  latency sums, mins, maxes, counts, and histogram bins are integers, so the
  merged totals do not depend on how lines were partitioned or in what order
  partials were folded (CR-1, CR-2).
- **Total-order ranking.** Every top-N table sorts by value descending then key
  ascending (byte-wise). Ties break deterministically, so output does not
  depend on hash-map iteration order or thread count (FR-TN-2).
- **Ordered time buckets.** `time_buckets` is a `std::map`; iteration is by
  bucket start.
- **stddev.** The only `double` reduction. Compared with relative tolerance
  `1e-6` in tests (CR-3); never used as a sort key.

### 4.5 Edge cases

| Case | Handling |
|---|---|
| Empty file | zero chunks, empty aggregate, empty report, exit 0. |
| File smaller than thread count | some chunks empty; their workers return empty partials. |
| Line longer than a nominal chunk | boundary adjustment lets that chunk exceed its nominal size. |
| No trailing newline | final line is still a whole line in the last chunk. |
| CRLF | trailing `\r` stripped by the format before field parsing. |
| `--threads 0` | resolved to `std::thread::hardware_concurrency()`. |
| `--exact-percentiles` with `--threads > 1` | forced to 1 thread with a warning on stderr. |

---

## 5. Timing and telemetry

`app` owns a `steady_clock` stopwatch. It records, separately:

- `open_map_ms` — file open + mmap (or fallback read);
- `phase_ms` — parse + filter + aggregate + merge;
- `render_ms` — building the report model + rendering.

`benchmark` additionally samples `PeakWorkingSetSize` via
`GetProcessMemoryInfo` after the phase, and derives records/second,
MB/second, speedup, and parallel efficiency from `phase_ms`.

---

## 6. Directory layout

```
log-analyzer/
├─ CMakeLists.txt              root: options, warnings, subdirs
├─ CMakePresets.json           windows-msvc preset (VS 2022 generator)
├─ SPEC.md                     the contract
├─ ARCHITECTURE.md             this file
├─ README.md                   usage + the multithreading discussion
├─ TEST_STATUS.md              per-milestone test tally
├─ docs/
│  └─ PERFORMANCE.md           benchmark methodology + measured results
├─ datasets/                   small checked-in fixtures; generated/ is ignored
├─ src/
│  ├─ main.cpp
│  ├─ app/  cli/  io/  parse/  filter/  aggregate/  stats/  concurrency/  report/  bench/
│  └─ <module>/CMakeLists.txt  one static lib per module
└─ tests/
   ├─ CMakeLists.txt           single loganalyzer_tests binary + CTest
   ├─ support/                 test_framework.hpp, runner_main.cpp
   ├─ data/                    fixtures + golden files
   └─ <module>/*_tests.cpp
```
