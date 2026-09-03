# Test Status

Updated at the end of every milestone. `cmake --build` must be clean and the
full `loganalyzer_tests` binary must be green before a milestone is marked done.

| Milestone | Date | Build | Tests (pass/total) | Notes |
|---|---|---|---|---|
| M0 — repo skeleton | 2026-09-02 | clean | 3/3 | smoke test only; CTest 1/1 |
| M1 — parser | 2026-09-02 | clean | 38/38 | timestamp + pipe format + analyze_buffer; CTest 1/1 |

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
