# Test Status

Updated at the end of every milestone. `cmake --build` must be clean and the
full `loganalyzer_tests` binary must be green before a milestone is marked done.

| Milestone | Date | Build | Tests (pass/total) | Notes |
|---|---|---|---|---|
| M0 — repo skeleton | 2026-09-02 | clean | 3/3 | smoke test only; CTest 1/1 |
| M1 — parser | 2026-09-02 | clean | 38/38 | timestamp + pipe format + analyze_buffer; CTest 1/1 |
| M2 — filters + CLI | 2026-09-02 | clean | 63/63 | RecordFilter + full analyze arg parsing; CTest 1/1 |
| M3 — aggregation + text report | 2026-09-02 | clean | 93/93 | sequential aggregate + stats + text renderer + golden-file integration test; CTest 1/1 |
| M4 — JSON report + strict + malformed samples | 2026-09-02 | clean | 103/103 | frozen JSON schema (`valid_small.expected.json`), `--exact-percentiles`, `--strict` exit 3; CTest 1/1 |
| M5 — io: mmap + chunk splitter | 2026-09-02 | clean | 116/116 | `MappedFile` (Win32 mmap + fallback), newline-aligned `split_into_chunks`; `analyze` now reads via mmap; CTest 1/1 |

## Detail — M0

Scope: CMake project, `windows-msvc` preset, `la_app` static lib with
`version_string()`, stub CLI (`version` / `help`), hand-rolled test framework,
one `loganalyzer_tests` binary wired to CTest.

Test cases:
- `version_string reports the project name`
- `version_string reports the version constant`
- `test framework arithmetic sanity`

Equivalence / determinism gates: not applicable yet (introduced at M6).

## Detail — M1

Scope: `la_parse` library — ISO-8601 timestamp parsing to epoch ms,
`Level` / `Method` enums, `ILogFormat` interface, `PipeDelimitedFormat`
(the v1 `pipe` grammar), `make_log_format`, and `analyze_buffer` /
`run_analyze` counting records, malformed lines, and blank lines. `analyze
<file>` now reports those counts.

Test files:
- `parse/timestamp_tests.cpp` — epoch anchors, pre-epoch, fractional
  truncation, leap-year rules, out-of-range calendar/clock fields, grammar
  violations (missing `Z`, wrong separators, trailing space, non-digits).
- `parse/log_record_tests.cpp` — level/method parse + `to_string`, level
  ordering, `reason_code` slugs, `is_blank_line`.
- `parse/pipe_format_tests.cpp` — fully populated HTTP line, non-HTTP line,
  all six levels case-insensitively, bare `|` in message, `" | "` absorbed
  into message, empty message, CRLF strip, whitespace trim, field-count
  error, and per-field rejection for timestamp/level/service/request/status/
  duration incl. half-to-even sub-microsecond rounding.
- `app/analyze_tests.cpp` — mixed valid/malformed/blank buffer, empty
  buffer, only-newlines, missing trailing newline.

Note: `LogRecord` string fields are views into the parsed buffer; tests hold
the line text in a named local (`Parsed` helper) for the result's lifetime.

## Detail — M2

Scope: `la_filter` (`FilterSpec` + compiled `RecordFilter` predicate) and
`la_cli` (`parse_args` -> `Options`, covering every documented `analyze`
flag plus benchmark/gen flags for later use). `app/run.cpp` dispatches
parsed commands; `main.cpp` is now a thin shell. `analyze_buffer` gained a
filter argument and a `kept` count.

Test files:
- `filter/record_filter_tests.cpp` — pass-through; half-open time range;
  `--level` threshold; `--level-only` exact set; service OR; status-class
  OR with missing-status exclusion; path prefix / substring with
  no-path exclusion; AND across categories; `is_pass_through`.
- `cli/parser_tests.cpp` — no command, version/help + topic, unknown
  command, file-arg arity, defaults, `--threads` validation (incl.
  negative and missing value), level filters incl. `--level-only`
  overriding `--level`, time bounds, `--status-class` Nxx/N/invalid,
  repeated `--service`, `--interval` units (s/m/h/bare), `--report`,
  `--top`, boolean flags + `-o`, unknown option.
- `app/analyze_tests.cpp` — updated for the filter argument; added a
  filter-narrows-`kept` case.

## Detail — M3

Scope: `la_core` (version, extracted from app to break a report->app cycle),
`la_aggregate` (`LatencyHistogram`, `EndpointStat`, `TimeBuckets`,
`Aggregate` + `aggregate_buffer`), `la_stats` (`build_report` -> render-ready
`Report` with total-order ranked lists), `la_report` (`render_text`).
`analyze` now aggregates and prints a full statistics report; `-o` writes to
a file. `format_timestamp` added as the inverse of `parse_timestamp`.

Also fixed: `duration_ms` was parsed as if the integer part were seconds
(x1e6); it is milliseconds (x1e3). Caught by eyeballing the first rendered
report.

Test files:
- `aggregate/histogram_tests.cpp` — inclusive upper edges, overflow bucket,
  merge, percentiles from a known fill, empty.
- `aggregate/endpoint_stat_tests.cpp` — counts/min/max/exact sum, population
  stddev, stddev==0 below 2 samples, merge + merge-with-empty identity.
- `aggregate/time_buckets_tests.cpp` — floored keys, negative-timestamp
  floor division, per-key merge.
- `aggregate/aggregate_tests.cpp` — line totals, by level/status/class,
  services/error-messages/failures, endpoint latency, time buckets, and a
  split-then-merge == whole check in both orders (pre-figures the M6 gate).
- `stats/report_tests.cpp` — severity rollup, ascending vs count-ranked
  codes, ranked services/errors, failures, busiest vs slowest ordering,
  `top_n` clamping, chronological timeline.
- `report/text_renderer_tests.cpp` — `format_interval`, all sections present
  for an empty report, latency sentinels ("-" / ">10000").
- `integration/analyze_text_test.cpp` — full `run()` pipeline over
  `tests/data/valid_small.log` diffed against `valid_small.report.txt`
  (golden, LF endings); missing-file exit code 2.

## Detail — M4

Scope: malformed-line samples (`Aggregate::malformed_samples`, bounded by
`--show-malformed`, line-sorted, merged as a bounded merge of two sorted
lists); `--exact-percentiles` (retain per-endpoint durations in
`EndpointStat::samples`, nearest-rank percentiles in `build_report`);
`--strict` (exit 3 after still rendering the report); `la_report`'s
`render_json` -- a hand-rolled streaming JSON writer producing a
pretty-printed, ASCII, LF object. JSON schema frozen: latency-ms sentinels
render as `null`. Text report gained a "malformed lines" section.

Test files added / extended:
- `aggregate/aggregate_tests.cpp` -- malformed sample line/reason/bounding,
  `collect_durations` sample retention, bounded sorted sample merge.
- `stats/report_tests.cpp` -- exact vs histogram-edge percentiles on a known
  set, malformed-sample passthrough with reason slugs.
- `report/json_renderer_tests.cpp` -- `json_quote` escaping (incl. control
  chars), empty-report well-formedness + balanced braces + empty-array
  collapse, latency `null` sentinels.
- `integration/analyze_text_test.cpp` -- JSON golden diff
  (`valid_small.expected.json`), `--strict` exit 3 with report still
  rendered.

## Detail — M5

Scope: `la_io`.
- `MappedFile::open` -- Win32 `CreateFileMapping` / `MapViewOfFile`, with a
  buffered-read fallback when mapping fails and a zero-length view for empty
  files; non-Windows always uses the buffered read. Move-only; the move
  re-points `data_` because the fallback buffer's address can change (SSO).
- `split_into_chunks` -- at most `n` contiguous pieces, each split point
  snapped forward to just past a `\n`; no empty chunks; exact cover; fewer
  than `n` chunks for small / newline-sparse buffers.
`run_analyze` now reads through `MappedFile` instead of an ifstream slurp.

New fixtures: `empty.log` (0 bytes), `only_newlines.log`,
`no_trailing_newline.log`, `crlf.log`.

Test files:
- `io/mapped_file_tests.cpp` -- exact bytes for a real file, empty file ok
  with zero-length view, missing file error, CRLF preserved, move keeps the
  view valid.
- `io/chunking_tests.cpp` -- empty buffer, `n<=1`, forward-snap behaviour,
  exact cover + interior-chunks-end-with-newline + no-empty-chunk invariants
  for n=1..16, no-newline buffer, one long line not split, no trailing
  newline, `n` > line count.
