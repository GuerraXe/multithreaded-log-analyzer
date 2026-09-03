# Datasets

Small, hand-checked fixtures live in [`../tests/data/`](../tests/data/) and are
committed:

| File | Purpose |
|---|---|
| `valid_small.log` | 14 lines covering every level, HTTP + non-HTTP, one malformed and one blank line |
| `valid_small.report.txt` / `valid_small.expected.json` | golden output for the integration tests (LF line endings) |
| `empty.log` | 0 bytes |
| `only_newlines.log` | blank-line handling |
| `no_trailing_newline.log` | last line without a `\n` |
| `crlf.log` | CRLF line endings preserved verbatim |

Large benchmark datasets are **not committed** — they are reproducible from a
seed, so anyone gets identical bytes:

```
loganalyzer gen bench_1m.log   --lines 1000000  --seed 1     #  ~86 MB
loganalyzer gen bench_5m.log   --lines 5000000  --seed 1     # ~432 MB
loganalyzer gen tiny.log       --lines 1500     --seed 1     # ~130 KB (overhead regime)
```

`gen` output is byte-identical across platforms and standard libraries (it
uses only `std::mt19937_64` plus hand-rolled uniform / Box-Muller transforms).
The generated mix and the exact commands used for the numbers in
[`../docs/PERFORMANCE.md`](../docs/PERFORMANCE.md) are documented there.

`*.log` is git-ignored (except the committed `tests/data/*.log`); put working
datasets under `perf/` or `datasets/generated/`, both ignored.
