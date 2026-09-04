# Steal or Wait? — Work-Stealing Scheduler Study

C HTTP/1.1 file server comparing four thread-pool scheduling policies using epoll edge-triggered I/O, `sendfile()` zero-copy transfers, and POSIX threads.

## Scheduling Policies

| Policy | Queue | Steal Strategy |
|--------|-------|----------------|
| **RR** (Round-Robin) | Mutex-protected FIFO per worker | None — main thread dispatches in rotation |
| **RS** (Random Steal) | Chase-Lev lock-free deque | Random victim, O(1) steal |
| **LQS** (Longest-Queue Steal) | Chase-Lev lock-free deque | Scan all deques, steal from fullest, O(p) steal |
| **AS** (Adaptive Steal) | Chase-Lev lock-free deque | Random victim + p99-driven EMA feedback loop |

## Quick Start

```bash
# Build (release)
make release

# Smoke test
./bin/server -p 8080 -r ./corpus -t 4 -s rs &
curl -o /dev/null -w "%{http_code}" http://localhost:8080/   # → 200
kill %1
```

## Build

```bash
make          # Debug build (default)
make release  # Release build (-O3 -march=native -flto -DNDEBUG)
make clean
```

Binary produced at `bin/server`.

## Usage

```
Usage: bin/server [options]
  -p, --port <port>          Port number (default: 9000)
  -r, --root <path>          Document root (default: ./public)
  -t, --threads <n>          Number of worker threads (default: 4)
  -s, --scheduler <type>     Scheduler: rr, rs, lqs, as (default: rr)
  -b, --steal-batch <mode>   Steal batch: 1 or half (default: 1)
  -h, --help                 Show this help
```

## Runtime Metrics

On shutdown, the server prints per-worker stats (task count, steal success rate, idle time, avg/max task duration, p50/p99 latency), global latency percentiles (p50/p95/p99/p99.9), and — for the lock-free schedulers (RS, LQS, AS) — a CAS Contention report showing failed compare-and-swap attempts per deque.

## Benchmarking

```bash
# Full benchmark suite (48 runs, ~13 minutes)
make bench

# Manual single run
./bin/server -p 8080 -r ./corpus -t 4 -s as &
wrk -t4 -c32 -d30s --latency http://localhost:8080/
kill %1
```

Results written to `bench_results/results.csv`.

## Analysis

```bash
# Requires: numpy, scipy, matplotlib
pip install numpy scipy matplotlib

# Generate plots and summary tables
make analyze
```

Reads `bench_results/results.csv` and produces PNG plots in `bench_results/plots/`:

| Plot | Description |
|------|-------------|
| `speedup.png` | Throughput + speedup vs RR 1T across all schedulers |
| `amdahl.png` | Amdahl's Law curve fit (parallel fraction estimate) |
| `latency.png` | P99 latency vs thread count |
| `4t_comparison.png` | Bar chart at 4 threads |
| `heatmap.png` | Throughput heatmap (scheduler × threads) |
| `scaling_strong.png` | Strong scaling speedup S(p) |
| `scaling_weak.png` | Weak scaling efficiency E(p) |

## Papers

PDFs for the conference paper and appendices are in `docs/`:

- `docs/paper.pdf` — main paper
- `docs/appendices.pdf` — appendices (Gantt chart + experimental logbook)

## Project Structure

```
├── main.c                  Entry point + epoll event loop
├── makefile                Build + bench + analyze targets
├── include/                Header files
│   ├── cli.h               CLI argument parsing
│   ├── deque.h             Chase-Lev lock-free deque
│   ├── hashtable.h         HTTP header hashtable
│   ├── http.h              HTTP request/response parsing
│   ├── metrics.h           Per-thread steal counters + latency histogram
│   ├── network.h           Socket / listener helpers
│   ├── queue.h             Mutex-protected FIFO (RR)
│   ├── thread_handler.h    Connection handler
│   ├── thread_pool.h       Thread pool + scheduler interface
│   └── utils.h             String utilities
├── src/                    Implementation files
├── tests/                  Component test drivers (make test-<name>)
├── scripts/
│   ├── run_bench.py        Automated benchmark runner
│   └── analyze.py          Results analysis + plots
├── docs/                   Paper, appendices
├── bench_results/          Benchmark output (CSV + plots)
└── corpus/                 Static file corpus for testing
```

## Environment (Benchmarked On)

| Component | Detail |
|-----------|--------|
| CPU | Intel Core i5-8350U @ 1.70 GHz (4C/8T, L3 6 MiB) |
| RAM | 15 GiB |
| OS | Arch Linux, kernel 7.0.12 |
| Compiler | gcc 16.1.1 |
| Load gen | wrk 4.2.0 [epoll] |
| Python | 3.x with numpy, scipy, matplotlib |
