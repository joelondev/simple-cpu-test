# How PartumTestKit Works

PartumTestKit is a small Cinebench-style CPU benchmark. It runs three
fixed, deterministic workloads, once on a single thread and once across
all available logical cores, then turns the measured wall-clock times into
scores.

This document explains the architecture, the workloads, the threading
model, the timing method, and how the scores are calculated.

## Repository layout

```
src/partum.h      shared types, prototypes and platform includes
src/func.c        timing, CPU detection and the parallel runner
src/workloads.c   the integer, float and render workloads
src/main.c        command-line parsing, phases, scoring and output
Makefile          build shortcuts
bin/              prebuilt macOS binaries
docs/             this document
```

## Execution flow

1. `main` parses the command line.
2. It asks the platform for the CPU name and logical core count.
3. It runs the **single-threaded phase** with one worker.
4. It runs the **multi-threaded phase** with one worker per logical core
   (or the value passed to `--threads`).
5. For every workload it keeps the fastest of `--runs` attempts
   (default: 3).
6. It converts each time into a score, adds the workload scores together,
   and prints either a human-readable report or JSON.

`--single` and `--multi` skip the other phase. `--quick` uses smaller
work sizes, but the same scoring constants, so quick results stay on the
same scale.

## Timing

`now_seconds()` is the only clock used for measurements:

- **Windows:** `QueryPerformanceCounter` / `QueryPerformanceFrequency`.
- **macOS and Linux:** `clock_gettime(CLOCK_MONOTONIC)`.

Each workload's timer starts immediately before its worker threads are
created and stops immediately after all of them have joined. File
allocation, matrix initialisation and checksum reduction are outside the
timed region so they do not distort the result.

Running each workload several times and keeping the best attempt reduces
noise from CPU frequency ramp-up, background processes and thermal
throttling.

## Threading model

All three workloads use the same helper:

```c
int run_parallel(struct parallel_task *tasks, int count);
```

A `parallel_task` is a function pointer plus an argument pointer. The
runner either:

- calls the function directly when `count == 1`, avoiding thread
  overhead for single-threaded runs, or
- starts `count` threads with `pthread_create` on POSIX or
  `_beginthreadex` on Windows, waits for all of them, and joins them.

Work is split into contiguous chunks. Each worker writes only to its own
argument struct, so no locks are needed. After the join, the main thread
combines the per-worker results into a single checksum.

Chunk sizes are rounded up:

```c
chunk = (total + threads - 1) / threads;
```

This keeps the work balanced even when there are more threads than items,
and the final chunk is clamped to the end of the range.

## Workloads

### Integer: prime counting

The integer workload counts primes in `[2, N]` using trial division:

- even numbers and multiples of 3 are rejected first;
- candidates are tested with `i = 5, 11, 17, ...` up to `sqrt(n)`.

The range is split evenly across the workers and each worker returns its
own prime count. The counts are summed into a checksum.

`N` is `10,000,000` in standard mode and `4,000,000` in quick mode.

Trial division does not cost a constant amount per number: larger numbers
need more divisors. To make the work unit realistic, the integer workload
reports

```
work = (2 / 3) * N^1.5
```

which approximates the total number of trial divisions. This keeps the
integer score consistent between standard and quick mode.

### Float: matrix multiplication

The float workload multiplies two dense `N x N` matrices of `double`
values:

```
C[i][j] += A[i][k] * B[k][j]
```

It uses the cache-friendly `i-k-j` loop order: the inner loop walks a row
of `B` and a row of `C` sequentially. Matrices are filled with a simple
deterministic pattern before timing starts.

`N` is `1024` in both standard and quick mode. The reported work is the
number of floating-point operations:

```
work = 2 * N^3
```

Rows are split across workers, and each worker returns the sum of its rows
of `C` as a checksum.

### Render: Mandelbrot set

The render workload evaluates the Mandelbrot recurrence

```
z = z^2 + c
```

for every pixel in a fixed image, with a fixed maximum iteration count.
Pixels are independent, so image rows are split across workers.

Standard mode uses `1600 x 1000` pixels and `1024` iterations; quick mode
keeps the same resolution with `256` iterations. The reported work is

```
work = width * height * max_iterations
```

Each worker adds a value derived from its pixels' iteration counts to a
checksum.

## Scoring

For every workload:

```
score = (work / seconds) / ops_per_point
```

`ops_per_point` is a fixed calibration constant per workload. It converts
raw throughput into a convenient, Cinebench-like range. The constants are:

| Workload | `ops_per_point` |
| --- | --- |
| Integer | `31,500,000` |
| Float | `7,000,000` |
| Render | `1,340,000` |

The single-threaded total is the sum of the three workload scores in the
1-thread phase. The multi-threaded total is the sum in the all-core phase.

- **Partum Score** is the multi-threaded total, or the single-threaded
  total when `--single` is used.
- **Scaling** is `multi total / single total`. On a hybrid CPU it is
  normal for this to be lower than the core count because efficiency cores
  are slower than performance cores.

Scores are relative units. They are designed to be comparable between runs
of PartumTestKit on different machines, not to Cinebench or any other
benchmark.

## Determinism and checksums

All workloads are deterministic: the same work produces the same checksum
regardless of thread count. The checksum is included in JSON output and
can be used to confirm that a run actually completed the expected work.

## Command-line reference

```
-q, --quick          Run a shorter test
-t, --threads <n>    Worker threads for the multi-core test (default: all)
-1, --single         Run only the single-threaded test
-m, --multi          Run only the multi-threaded test
-r, --runs <n>       Runs per workload, best time kept (default: 3)
    --json           Print machine-readable JSON results
-h, --help           Show this help
-v, --version        Show the version
```

## JSON output

```json
{
  "tool": "PartumTestKit",
  "version": "1.0.0",
  "cpu": "Apple M1",
  "cores": 8,
  "threads": 8,
  "quick": false,
  "partum_score": 14564,
  "scaling": 3.44,
  "results": {
    "single": {
      "threads": 1,
      "score": 4228,
      "workloads": [
        { "name": "Integer", "seconds": 0.4780, "work": 21081851068, "score": 1400, "checksum": 664579 }
      ]
    },
    "multi": {
      "threads": 8,
      "score": 14564,
      "workloads": [
        { "name": "Integer", "seconds": 0.1352, "work": 21081851068, "score": 4950, "checksum": 664579 }
      ]
    }
  }
}
```

`threads` is `null` when only the single-threaded phase ran. The workload
arrays always contain three entries in the real output.

## Cross-platform notes

- POSIX builds need `pthread`; Linux builds may also need `-lm`.
- Windows builds need `<windows.h>` and `<process.h>`; `_beginthreadex`
  is used instead of `pthread_create`.
- CPU names come from `sysctlbyname` on macOS, `/proc/cpuinfo` on Linux,
  and CPUID on Windows.
- Logical core count comes from `sysconf(_SC_NPROCESSORS_ONLN)` on POSIX
  and `GetNativeSystemInfo` on Windows.

## Adding a workload

1. Add a `run_*` function and a `*_work_units` function to
   `src/workloads.c`.
2. Declare both in `src/partum.h`.
3. Add an entry to the `WORKLOADS` table in `src/main.c` with a name,
   detail string, function pointers and an `ops_per_point` constant.
4. Choose a fixed work size for standard mode and a smaller one for quick
   mode, and make the work estimate proportional to the real cost.
5. Add a checksum so the run can be verified.

## Stability tips

- Use `-r 5` or higher for the most repeatable scores.
- Close heavy background applications.
- Let the machine cool between runs.
- Plug laptops in and use the same power mode when comparing results.
