# Test Status

Updated at the end of every milestone. `cmake --build` must be clean and the
full `loganalyzer_tests` binary must be green before a milestone is marked done.

| Milestone | Date | Build | Tests (pass/total) | Notes |
|---|---|---|---|---|
| M0 — repo skeleton | 2026-09-02 | clean | 3/3 | smoke test only; CTest 1/1 |

## Detail — M0

Scope: CMake project, `windows-msvc` preset, `la_app` static lib with
`version_string()`, stub CLI (`version` / `help`), hand-rolled test framework,
one `loganalyzer_tests` binary wired to CTest.

Test cases:
- `version_string reports the project name`
- `version_string reports the version constant`
- `test framework arithmetic sanity`

Equivalence / determinism gates: not applicable yet (introduced at M6).
