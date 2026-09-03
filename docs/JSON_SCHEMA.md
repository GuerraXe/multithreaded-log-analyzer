# `--report json` schema (frozen at M4)

A single JSON object, pretty-printed with 2-space indentation, ASCII only, LF
line endings. The canonical example is
[`tests/data/valid_small.expected.json`](../tests/data/valid_small.expected.json),
which the test suite diffs byte-for-byte.

## Top level

| Key | Type | Notes |
|---|---|---|
| `tool` | string | `"loganalyzer X.Y.Z"` |
| `top_n` | integer | the `--top` value ranked lists were clamped to |
| `exact_percentiles` | boolean | whether latency percentiles are exact (sorted samples) or histogram-edge estimates |
| `input` | object | `bytes`, `lines` (non-blank), `records`, `kept`, `malformed`, `blank` — all integers |
| `severity` | object | `TRACE`..`FATAL` counts, plus `errors` (Error+Fatal) and `warnings` (Warn) |
| `status_classes` | object | `1xx`..`5xx` counts |
| `response_codes` | array | every status code seen, ascending: `{ "code": int, "count": int }` |
| `top_status_codes` | array | same shape, ranked by count desc then code asc, clamped to `top_n` |
| `top_services` | array | `{ "service": string, "count": int }`, ranked |
| `top_errors` | array | `{ "message": string, "count": int }`, ranked (records with level ≥ ERROR) |
| `failures_by_service` | array | `{ "service": string, "count": int }`, ranked (level ≥ ERROR or 5xx) |
| `busiest_endpoints` | array | endpoint objects, ranked by request count |
| `slowest_endpoints` | array | endpoint objects with ≥1 timed request, ranked by mean latency |
| `timeline` | object | `interval_ms` (int) and `buckets` (array) |
| `malformed_samples` | array | `{ "line": int, "reason": string, "text": string }`, up to `--show-malformed` entries, line-sorted |

## Endpoint object

```json
{
  "endpoint": "GET /v1/users/42",
  "count": 3,          // requests, with or without a duration
  "timed": 3,          // requests that carried a duration
  "mean_ms": 7.8667,   // fixed 4 decimals; 0 when timed == 0
  "stddev_ms": 4.2944, // population stddev, fixed 4 decimals
  "min_ms": 2,
  "max_ms": 12,
  "p50_ms": 10,
  "p90_ms": 20,
  "p99_ms": 20
}
```

`min_ms` / `max_ms` / `p50_ms` / `p90_ms` / `p99_ms` are integers, **or `null`**:

- `null` when the endpoint has no timed requests.
- `null` (histogram mode only) when the percentile falls above the 10 000 ms
  histogram ceiling. Use `--exact-percentiles` for a precise value.

## Timeline bucket object

```json
{ "start": "2026-03-01T09:00:00Z", "requests": 7, "errors": 3 }
```

`start` is the bucket's inclusive lower bound, ISO-8601 UTC.

## Stability

Field order is fixed by the renderer. Strings are JSON-escaped (`"`, `\`,
`\b \f \n \r \t`, and `\u00XX` for other control characters). Everything except
`mean_ms` / `stddev_ms` is an exact integer or an ordered string, so the output
is byte-stable across runs and (from M6) across thread counts.
