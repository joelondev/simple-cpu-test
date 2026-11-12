# PartumTestKit

A small, Cinebench-style CPU benchmark written in C. It runs three
deterministic workloads and reports single-threaded and multi-threaded
scores so you can compare machines, cooling, power modes, or settings.

See [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md) for the architecture,
workloads, threading model and scoring details.

## What it measures

| Workload | What it does |
| --- | --- |
| Integer | Trial-division prime counting up to a fixed limit |
| Float | Double-precision matrix multiplication |
| Render | Mandelbrot set at a fixed resolution and iteration depth |

Each workload is run twice: once on a single thread (`1T`) and once across
all available logical cores (`nT`). The same total amount of work is used in
both runs, so the wall-clock time drops as parallelism increases.

## Build

With `make`:

```sh
make
```

Or directly:

```sh
clang src/*.c -O2 -o partumtestkit
gcc   src/*.c -O2 -lm -lpthread -o partumtestkit
```

Prebuilt macOS binaries live in `bin/macOS-arm64/` and `bin/macOS-x86-64/`.
On Windows, build `src/*.c` with MSVC or clang-cl.

## Usage

```sh
./partumtestkit
```

```
PartumTestKit 1.0.0
CPU    Apple M1
Cores  8 logical
Mode   standard (best of runs)

Running single-threaded test...

Single-threaded (1 thread)
  Integer   prime counting        0.529 s       1265 pts
  Float     matrix multiply       0.206 s       1493 pts
  Render    mandelbrot            1.005 s       1216 pts
  Total                                       3974 pts

Running multi-threaded test with 8 threads...

Multi-threaded (8 threads)
  Integer   prime counting        0.137 s       4869 pts
  Float     matrix multiply       0.055 s       5568 pts
  Render    mandelbrot            0.332 s       3687 pts
  Total                                      14123 pts

Scaling          3.55x
Partum Score        14123 pts
```

### Options

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

Examples:

```sh
./partumtestkit --quick
./partumtestkit --threads 4 --runs 5
./partumtestkit --json > result.json
```

## Scores

Scores are relative units, not physical measurements. Each workload has a
fixed calibration constant, so a given machine should produce roughly the
same score for the same workload size. `Partum Score` is the multi-threaded
total (or the single-threaded total when `--single` is used). `Scaling` is
the multi-threaded total divided by the single-threaded total.

Because of that calibration, scores are comparable between runs of
PartumTestKit on different machines. They are **not** comparable to
Cinebench or other benchmark scores.

## Notes

- Use `--runs` (for example `-r 5`) and a cool machine for the most stable
  results. The default takes the best of three runs per workload.
- On hybrid CPUs, all logical cores are used, so scaling depends on how
  many performance and efficiency cores the machine has.
- The original repository was a simple prime-crunching CPU test by
  Fredrik B, later modified by @joelondev. PartumTestKit is a rewrite that
  adds single/multi-thread scoring and additional workloads.
