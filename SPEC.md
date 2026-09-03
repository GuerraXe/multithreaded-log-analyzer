# Multithreaded Log Analyzer — Specification

Status: approved 2026-09-02. This document is the contract the implementation
and tests are held to. Changes to a frozen section require an explicit note in
the milestone log and a matching test update.

---

## 1. Purpose and scope

`loganalyzer` is a command-line tool that ingests large application / server
log files and produces operational statistics: error and warning counts, the
most frequent errors, slow endpoints, HTTP status-code mix, traffic over time,
and failure counts per service. It offers **sequential** and **multithreaded**
execution of the same analysis so their performance can be measured and
compared.

### In scope (v1)

- Parse one structured log file (format `pipe`, section 3) into typed records.
- Detect, count, and sample malformed lines; exclude them from statistics.
- Filter by time range, severity, service, HTTP status class, and endpoint path.
- Aggregate counts, error-message frequencies, per-endpoint latency statistics,
  and time-window buckets.
- Top-N queries with a deterministic tie-break.
- `text` and `json` reports.
- `analyze` (sequential or `--threads N`) and `benchmark` (thread-count sweep).
- Performance telemetry: record count, malformed count, phase wall time,
  records/second, MB/second, thread count, peak working-set memory.

### Out of scope (v1)

Live tail / follow mode; multi-file or glob input; compressed input; regex,
logfmt, JSON, or syslog formats (the parser interface leaves room for them);
distributed processing; a graphical interface.

---

## 2. Definitions

| Term | Meaning |
|---|---|
| Record | One successfully parsed log line, represented as a `LogRecord`. |
| Malformed line | A non-empty line that fails structural or field validation. |
| Blank line | A line that is empty or contains only whitespace. Skipped; **not** counted as malformed. |
| Phase time | Wall-clock time for parse + filter + aggregate only. Excludes process start, file open/map, and report rendering, which are timed separately. |
| Aggregate | The merged result of all per-thread partial aggregates. |
| Report model | The pure data structure produced by `stats` from an `Aggregate`; rendered by `report`. |

---

## 3. Supported log format: `pipe`

One record per line. Fields are separated by the three-character sequence
`space pipe space` (`" | "`). Exactly seven fields:

```
<timestamp> | <level> | <service> | <request> | <status> | <duration_ms> | <message>
```

Example lines:

```
2026-08-14T12:34:56.789Z | INFO  | api-gateway   | GET /v1/users/42 | 200 | 13.4  | request completed
2026-08-14T12:34:57.001Z | ERROR | order-service | POST /v1/checkout | 402 | 88.2  | payment declined: card_declined
2026-08-14T12:34:57.010Z | WARN  | scheduler     |                  |     |       | job queue depth high (512)
```

### 3.1 Field rules

| Field | Rule | May be empty |
|---|---|---|
| `timestamp` | ISO-8601 UTC, `YYYY-MM-DDThh:mm:ss` with optional `.fff` fractional seconds, trailing `Z` required. Stored as `int64` Unix epoch **milliseconds**. | no |
| `level` | One of `TRACE DEBUG INFO WARN ERROR FATAL`, case-insensitive on input, normalized to upper case. | no |
| `service` | Matches `[A-Za-z0-9._-]+`. Surrounding spaces are trimmed before validation. | no |
| `request` | Empty, **or** `METHOD` `SP` `path` where `METHOD` is one of `GET POST PUT PATCH DELETE HEAD OPTIONS` and `path` begins with `/`. Surrounding spaces trimmed. | yes |
| `status` | Empty, **or** an integer in `[100, 599]`. | yes |
| `duration_ms` | Empty, **or** a non-negative decimal number. Stored internally as integer **microseconds**, round half to even. | yes |
| `message` | Free text to end of line. May contain `\|`; may not contain the separator `" \| "`. Trailing `\r` (CRLF input) is stripped. Leading/trailing spaces trimmed. May be empty. | yes |

### 3.2 Malformed classification

A non-empty, non-blank line is **malformed** when any of the following holds:

- fewer than six `" | "` separators precede the message (fewer than 7 fields);
  a line with six or more separators is accepted, and any beyond the sixth are
  taken as part of the `message` value (see section 3.3);
- `timestamp` does not match the grammar or encodes an invalid date/time;
- `level` is not a recognized level name;
- `service` is empty after trimming or contains a disallowed character;
- `request` is non-empty and does not match the `METHOD SP /path` grammar;
- `status` is non-empty and is not an integer in `[100, 599]`;
- `duration_ms` is non-empty and is not a non-negative decimal.

For each malformed line the analyzer records: 1-based line number, a short
reason code, and the raw line text truncated to 256 bytes. At most
`--show-malformed N` samples are retained for display (default 5); the **count**
of malformed lines is always exact.

### 3.3 Documented limitations

- A literal `" | "` inside `message` causes that message to be split; because
  `message` is the final field, only its own value is affected (fields 1–6 are
  unaffected). No escaping mechanism in v1.
- Input is treated as bytes. UTF-8 passes through untouched; no transcoding or
  normalization is performed.
- Timestamps are assumed UTC. A non-`Z` zone suffix is malformed in v1.

---

## 4. Functional requirements

### 4.1 Input

- `FR-IN-1` Read the target file via memory mapping; fall back to buffered
  stream reads if mapping fails. Behavior and results are identical either way.
- `FR-IN-2` An empty file (0 bytes) produces an empty report with all counts 0
  and exit code 0.
- `FR-IN-3` A file whose final line has no trailing newline still has that line
  parsed.
- `FR-IN-4` A missing or unreadable file produces a diagnostic on stderr and
  exit code 2.

### 4.2 Parsing

- `FR-PA-1` Each non-blank line is parsed by the selected `ILogFormat` into a
  `LogRecord` or classified malformed per section 3.2.
- `FR-PA-2` Blank lines are skipped and counted separately from malformed lines.
- `FR-PA-3` Parsing never aborts on a malformed line; it is recorded and
  processing continues.

### 4.3 Filtering

Filters combine with logical AND across categories. Within a repeatable
category (`--service`, `--status-class`) the values combine with OR.

- `FR-FI-1` `--from <ts>` / `--to <ts>`: keep records with
  `from <= timestamp < to` (half-open). Either bound may be omitted.
- `FR-FI-2` `--level <LVL>`: keep records with severity `>= LVL` in the order
  `TRACE < DEBUG < INFO < WARN < ERROR < FATAL`.
- `FR-FI-3` `--level-only <L,L,...>`: keep records whose level is exactly in the
  given set. Overrides `--level` if both are supplied.
- `FR-FI-4` `--service <name>` (repeatable): keep records whose service equals
  any given name.
- `FR-FI-5` `--status-class <Nxx>` (repeatable): keep records whose status is in
  class `N` (`1xx`..`5xx`). Records with no status are excluded when this filter
  is active.
- `FR-FI-6` `--path-prefix <p>`: keep records whose request path starts with
  `p`. Records with no path are excluded when active.
- `FR-FI-7` `--path-contains <s>`: keep records whose request path contains `s`.
  Records with no path are excluded when active.
- `FR-FI-8` With no filters, all records are kept.

### 4.4 Aggregation and statistics

Computed over records that pass the filter:

- `FR-AG-1` Total records, kept records, malformed count, blank count, bytes
  processed.
- `FR-AG-2` Count by level (all six levels, zero-filled).
- `FR-AG-3` Count by exact status code and by status class.
- `FR-AG-4` Count by service.
- `FR-AG-5` Error-message frequency: for records with level `>= ERROR`, a
  frequency table keyed by the exact `message` string.
- `FR-AG-6` Per-endpoint statistics keyed by `METHOD SP path`: request count,
  latency min / max / mean / standard deviation, and p50 / p90 / p99 from a
  fixed-bucket histogram. Records with no duration contribute to the count but
  not to latency statistics.
- `FR-AG-7` Failure count per service: records with level `>= ERROR` or status
  in class `5xx`.
- `FR-AG-8` Time buckets of width `--interval` (default `1m`): per bucket,
  request count and error count (level `>= ERROR` or status `5xx`). Bucket key
  is the floor of the timestamp to the interval, in epoch milliseconds.

### 4.5 Top-N

- `FR-TN-1` `--top N` (default 10) bounds every ranked table: top errors,
  busiest endpoints, slowest endpoints (by mean latency, minimum 1 request),
  top services by failure count, top status codes.
- `FR-TN-2` Ranking order is value descending, then key ascending
  (lexicographic, byte-wise). This total order makes output independent of
  input order and thread count.
- `FR-TN-3` If fewer than `N` keys exist, all are returned.

### 4.6 Reporting

- `FR-RE-1` `--report text` (default): a human-readable multi-section summary.
- `FR-RE-2` `--report json`: a single JSON object. The schema is frozen at
  milestone M4 and captured in `tests/data/valid_small.expected.json`.
- `FR-RE-3` `-o <file>` writes the report to `<file>`; otherwise stdout.
- `FR-RE-4` Report content (excluding an explicitly labeled timing section) is a
  pure function of the input bytes and options.

### 4.7 Commands

- `FR-CMD-analyze` `loganalyzer analyze <file> [options]` — run the analysis and
  print a report.
- `FR-CMD-benchmark` `loganalyzer benchmark <file> [options]` — run the analysis
  repeatedly across a set of thread counts and report throughput, speedup, and
  parallel efficiency, plus a plain-language verdict on whether threading helped
  for this input.
- `FR-CMD-gen` `loganalyzer gen <file> [options]` — write a synthetic dataset
  with a fixed seed and known statistical properties, for tests and benchmarks.
- `FR-CMD-version` / `FR-CMD-help` — version string; usage text.

### 4.8 Exit codes

| Code | Meaning |
|---|---|
| 0 | Success. |
| 1 | Usage error (unknown command or option, missing argument, invalid option value). |
| 2 | I/O error (file missing, unreadable, or write failure). |
| 3 | `--strict` was given and at least one malformed line was found. |

---

## 5. Non-functional requirements

- `NFR-1` Language: C++20. Toolchain: MSVC v143 (VS 2022 Build Tools) via
  `CMakePresets.json`. Warning-clean at `/W4 /permissive-`.
- `NFR-2` No third-party runtime or test dependencies. The test framework is
  the in-repo header in `tests/support/`.
- `NFR-3` No global mutable state. All collaborators are constructed and passed
  explicitly. The only permitted `static` mutable is the test registry.
- `NFR-4` Determinism: for fixed input and options, `text` and `json` output
  (excluding the timing section) are byte-for-byte reproducible across runs.
- `NFR-5` Sequential / multithreaded equivalence (see section 6).
- `NFR-6` Memory: peak additional resident memory is `O(mapped file) +
  O(distinct keys)`; records hold `string_view`s into the mapped buffer and do
  not copy line text. Aggregates copy only retained keys and messages.

---

## 6. Correctness requirements — sequential vs multithreaded

Let `R_seq` be the report model from a run with one thread and `R_par(t)` the
report model from a run with `t` threads, same input and same options.

- `CR-1` The following fields of `R_par(t)` are **bitwise identical** to
  `R_seq` for every `t` in `1..64` and every chunk arrangement:
  all counts (records, kept, malformed, blank, by level, by status, by status
  class, by service, by time bucket, per-endpoint request counts, failures per
  service); latency **min**, **max**, and **sum** (accumulated as integer
  microseconds); latency **histogram bin counts**; every **top-N list**
  including order.
- `CR-2` Latency **mean** equals `sum_us / count` computed from the identical
  integer `sum_us` and `count`, hence also bitwise identical.
- `CR-3` Latency **standard deviation** is computed in `double` and compared
  with relative tolerance `1e-6`. This is the only value permitted to differ
  between modes, and the tolerance is asserted by tests.
- `CR-4` The merge of partial aggregates is associative and commutative:
  `merge(a, b) == merge(b, a)`, `merge(a, empty) == a`, and
  `merge(merge(a, b), c) == merge(a, merge(b, c))` for all counted fields.
- `CR-5` Chunk splitting assigns every byte of the file to exactly one chunk,
  split points fall on line boundaries, and no line is processed by more than
  one worker. Verified for: empty buffer, buffer with no trailing newline,
  CRLF endings, and chunk count greater than line count.
- `CR-6` A run with `t` greater than the number of non-empty chunks produces
  the same result as a run where every chunk is non-empty; surplus workers
  contribute an empty partial aggregate.

---

## 7. Performance methodology (summary; full text in `docs/PERFORMANCE.md`)

- Datasets are generated with `loganalyzer gen` at a fixed seed, in sizes
  10 MB / 100 MB / 1 GB, with a realistic mix (≈85% INFO, 10% WARN, 4% ERROR,
  1% FATAL; ≈70% of lines carry an HTTP request with a Zipfian endpoint
  distribution; log-normal latencies; monotonically increasing timestamps).
- For each thread count: `--warmup 1` discarded run, then `--repeat 5` measured
  runs; report the **median** phase time with min and max. Timer is
  `std::chrono::steady_clock`.
- Metrics: records, malformed, phase wall time, records/second, MB/second,
  speedup versus one thread, parallel efficiency (speedup / threads), merge
  time as a percentage of phase time, and peak working-set bytes
  (`GetProcessMemoryInfo`).
- Cold-cache and warm-cache (file already in the OS page cache) results are
  reported separately. The cold run is I/O-bound and is expected to show little
  parallel benefit.
- A small-input case (~100 KB) is included to demonstrate the regime where
  thread-creation and merge overhead exceed the work and threading is slower.
- Benchmarks are run only after all correctness gates pass.

---

## 8. Milestones

| # | Deliverable |
|---|---|
| M0 | Repo skeleton: CMake + presets + `.gitignore`, stub CLI (`version`/`help`), test binary with a passing smoke test, `SPEC.md` + `ARCHITECTURE.md`. Builds green. |
| M1 | Timestamp parsing, `LogRecord`, `ILogFormat`, `PipeDelimitedFormat`; exhaustive parser tests; `analyze` prints record and malformed counts. |
| M2 | `RecordFilter` and full `analyze` option parsing; filter and CLI tests; `analyze` applies filters. |
| M3 | Sequential aggregation, statistics models, `text` report; golden-file integration tests. Correctness baseline. |
| M4 | `json` report, `--exact-percentiles`, `--strict`, malformed samples; freeze JSON schema; check in `valid_small.expected.json`. |
| M5 | `io` memory mapping and newline-aligned chunk splitter; splitter tests; sequential path consumes chunks (n = 1). |
| M6 | `parallel_aggregate`, `--threads`, `PartialAggregate::merge`; sequential/multithreaded equivalence suite (1..64 threads). Headline correctness gate. |
| M7 | `benchmark` command, `gen` dataset generator, memory reporting; `docs/PERFORMANCE.md`. |
| M8 | Run benchmarks on the target machine, capture numbers, write the scaling and small-input analysis; one documented bottleneck fix (before/after). |
| M9 | Polish: `README.md` (why multithreading, work division, synchronization strategy, tradeoffs, benchmark methodology, limitations), `ARCHITECTURE.md` diagrams, `--help` text, warning-clean, full suite green, tag `v1.0`. |

Each milestone: tests written with the implementation; `cmake --build` clean;
full suite green; `TEST_STATUS.md` updated.
