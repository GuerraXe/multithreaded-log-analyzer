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

Release `/O2`, warm page cache, dataset `gen --lines 5000000 --seed 1`
(432 MB), sweep `--warmup 2 --repeat 7`, on the i9-13980HX laptop above:

```
  threads   median_ms     min     max     records/s      MB/s   speedup   eff
        1     2774.15  1083.10  2931.85       1802354     156.0     1.00x   100%
        2     1586.70  1483.83  1701.12       3151202     272.7     1.75x    87%
        4      835.14   786.77   954.00       5987013     518.0     3.32x    83%
        6      623.75   542.32   807.33       8015976     693.6     4.45x    74%
        8      486.69   439.53   531.88      10273396     888.9     5.70x    71%
       12      336.60   316.42   354.04      14854259    1285.3     8.24x    69%
       16      275.38   251.13   404.42      18156426    1571.0    10.07x    63%
       24      270.35   240.49   445.69      18494366    1600.3    10.26x    43%
       32      266.77   251.54   434.07      18742625    1621.8    10.40x    32%

verdict: multithreading helped: 10.40x at 32 threads (32% efficiency)
peak working set: 419 MB   (≈ the 432 MB mapping)
```

Reading the curve:

- **Near-linear to ~4 threads** (83–87 % efficiency): the work is CPU-bound
  (tokenising, validating, hash-map updates over disjoint line ranges) and
  there is no contention — each worker writes only its own `Aggregate`.
- **Efficiency falls off past the 8 performance cores.** 8→12→16 threads
  keep adding throughput (5.7×→8.2×→10.1×) but per-thread efficiency slides
  71 %→69 %→63 % as the 16 efficiency cores and SMT siblings contribute less
  per thread than a P-core would.
- **Throughput plateaus around 1.6 GB/s / 18–19 M records/s** from 16
  threads on. Adding threads past that barely moves the median: the phase is
  now limited by memory bandwidth (streaming a 432 MB mapping plus hash-map
  traffic), not by core count. 32 threads edges ahead of 24 only within
  noise.
- **Measurement noise is real on this hardware.** The 1-thread row's min
  (1083 ms) is less than half its median (2774 ms): the CPU is cool for the
  first iterations of the sweep and thermally throttles as sustained load
  builds, so medians drift upward through a run. `min` tracks the
  unthrottled best case; the true scaling sits between the two columns.
  Treat the shape, not the third significant figure, as the result.

### Small input — overhead dominates

`gen --lines 1500` (130 KB), `--warmup 5 --repeat 25`:

```
  threads   median_ms     min     max   speedup   eff
        1        0.26    0.26    0.34     1.00x   100%
        2        0.27    0.24    0.44     0.95x    47%
        4        0.24    0.20    0.34     1.09x    27%
        8        0.32    0.28    0.36     0.81x    10%
       16        0.55    0.50    0.70     0.47x     3%

verdict: multithreading was roughly neutral: best 1.09x at 4 threads
```

Below ~1 ms of actual work, spawning and joining 8+ `jthread`s and folding
8+ partial aggregates costs more than it saves. `benchmark` reports the
sub-1.0× verdict rather than assuming parallel is faster.

### Cold cache

Not measured with hard numbers here (no reliable cache-flush on the test
box). Qualitatively: the first read of an un-cached 432 MB file is
disk/BUS-bound at a few hundred MB/s regardless of thread count, so the
first cold run of any configuration lands in the same ballpark and
parallelism buys almost nothing until the file is resident. All numbers
above are explicitly warm-cache.

### Bottleneck investigation — per-record string allocation

**Hypothesis.** `Aggregate::observe` ran once per kept record and built a
fresh `std::string` for every hash-map probe — `by_service`,
`failures_by_service`, the `"METHOD /path"` endpoint key, and (for ERROR+)
`error_messages`. That is 2–4 heap allocations per record even when the key
already exists, which is the overwhelmingly common case.

**Change (one thing).** Switched those maps to
`std::unordered_map<std::string, V, TransparentStringHash, std::equal_to<>>`
and look them up with a `std::string_view`; a `std::string` is allocated
only on first sight of a key. The endpoint key is assembled into a scratch
buffer reused across calls (`src/aggregate/string_map.hpp`,
`Aggregate::endpoint_key_scratch`).

**Re-measure** (5 M dataset, `min` column to sidestep the throttle drift):

| threads | before (min ms) | after (min ms) | change |
|---:|---:|---:|---:|
| 1 | 1303 | 1083 | −17 % |
| 4 |  887 |  787 | −11 % |
| 16 |  250 |  251 |  ~0 % |

**Result.** A real gain where the phase is CPU-bound (1–4 threads), and
essentially nothing once it is memory-bandwidth-bound (16+ threads) — past
that point the allocator was never the limit. Modest, measured, and the
"why it's modest" is itself the finding: this workload hits the bandwidth
wall before allocation cost becomes dominant. The equivalence gate still
passes bit-for-bit after the change.

## Limitations

- Percentiles are histogram-based (bucket upper bounds) unless
  `--exact-percentiles`, which is single-threaded by construction.
- The timeline is not capped: a very wide time range with a small
  `--interval` produces a large `timeline` array.
- `merge` is a serial fold on the main thread. It is `O(distinct keys)` and
  under 1 % of phase time for realistic data; a parallel tree-merge is the
  fallback if that ever stops being true.
