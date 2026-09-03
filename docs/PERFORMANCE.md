# Performance methodology and results

Correctness is established first (see the equivalence gate in
[`TEST_STATUS.md`](../TEST_STATUS.md)); only then are the benchmarks below run.
Every measurement is reproducible from a clean checkout with the commands
shown.

## What is measured

`loganalyzer benchmark <file>` times **one thing**: the parse + filter +
aggregate + merge *phase*, i.e. a single call to `parallel_aggregate` over an
already-mapped buffer. Deliberately excluded and, where relevant, reported
separately:

- process startup and CLI parsing;
- opening and memory-mapping the file (done once, before timing);
- building the `Report` model and rendering text / JSON.

The timer is `std::chrono::steady_clock`. For each thread count the harness
runs `--warmup` discarded iterations, then `--repeat` measured iterations, and
reports the **median** wall time with the min and max. Derived figures:

| Metric | Definition |
|---|---|
| `records/s` | records processed ÷ median phase seconds |
| `MB/s` | file bytes ÷ 10⁶ ÷ median phase seconds |
| `speedup` | baseline (lowest thread count) median ÷ this median |
| `efficiency` | speedup ÷ thread count |

Peak working set is sampled once after the sweep via
`K32GetProcessMemoryInfo`.

## Dataset

Generated, fixed seed, so anyone can reproduce the exact bytes:

```
loganalyzer gen bench_1m.log   --lines 1000000  --seed 1
loganalyzer gen bench_10m.log  --lines 10000000 --seed 1
```

The generator (`src/gen/generator.cpp`) approximates a busy web service:
≈85 % INFO / 10 % WARN / 4 % ERROR / 1 % FATAL; ≈70 % of lines carry an HTTP
request with a Zipfian endpoint distribution; latencies are log-normal
(median ≈ 20 ms, tail into seconds); timestamps increase monotonically. No
malformed or blank lines.

## Protocol

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
./build/windows-msvc/src/Release/loganalyzer benchmark bench_10m.log \
    --threads-list 1,2,4,8,16,24,32 --warmup 2 --repeat 7
```

- **Release** build (`/O2`), MSVC v143.
- Warm cache: the file is read once to pull it into the OS page cache before
  the timed sweep; a separate cold-cache run (reboot or cache flush) is
  reported for contrast.
- The machine, core topology, RAM and OS build are recorded alongside the
  numbers.

## Environment

| | |
|---|---|
| CPU | 13th Gen Intel Core i9-13980HX — 24 cores (8 performance + 16 efficiency), 32 threads |
| OS | Windows 11 Pro, build 26200 |
| Toolchain | MSVC 19.44 (v143), CMake 3.31, `/O2` Release |

## Results

> _To be filled in at milestone M8 from a Release-build sweep on the machine
> above. The table below is the shape the `benchmark` command emits._

```
  threads   median_ms     min     max     records/s      MB/s   speedup   eff
        1         ...     ...     ...           ...       ...     1.00x   100%
        2         ...     ...     ...           ...       ...       ...x    ..%
        4         ...
        8         ...
       16         ...
       24         ...
       32         ...

verdict: ...
peak working set: ... MB
```

### Expected shape of the curve

- **Near-linear** speedup while the workload is CPU-bound and memory
  bandwidth is not saturated — roughly up to the 8 performance cores.
- **A visible kink** past 8 threads: the efficiency cores add throughput but
  at a lower per-thread rate, so `speedup` keeps rising while `efficiency`
  drops faster.
- **A plateau** once memory bandwidth (streaming the mapped file plus
  hash-map traffic) becomes the limit; adding threads past that point buys
  little.
- **Small inputs lose**: for a ~100 KB file the thread-creation and
  merge overhead exceeds the work, and `benchmark` reports a sub-1.0×
  verdict. Demonstrated with:
  ```
  loganalyzer gen tiny.log --lines 1500 --seed 1
  loganalyzer benchmark tiny.log --threads-list 1,8
  ```
- **Cold cache** is I/O-bound: the first read dominates and parallelism
  barely helps. This is a real limitation, reported separately rather than
  hidden by always measuring warm.

### Bottleneck investigation

> _M8: one worked example — measure, hypothesise, change one thing,
> re-measure. Candidates already visible in profiles: per-record
> `std::string` construction for hash-map keys, `unordered_map` rehashing
> under load, and timestamp parsing cost._

## Limitations

- Percentiles are histogram-based (bucket upper bounds) unless
  `--exact-percentiles`, which is single-threaded by construction.
- The timeline is not capped: a very wide time range with a small
  `--interval` produces a large `timeline` array.
- `merge` is a serial fold on the main thread. It is `O(distinct keys)` and
  under 1 % of phase time for realistic data; a parallel tree-merge is the
  fallback if that ever stops being true.
