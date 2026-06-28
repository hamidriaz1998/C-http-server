# Work-Stealing Scheduler Study: HTTP/1.1 Server

## Overview

C HTTP/1.1 server comparing four thread-pool scheduling policies:
- **RR** (Round Robin) — mutex-based per-worker task queue with lock-steering
- **RS** (Random Steal) — Chase-Lev lock-free deque, random victim, fixed 50µs idle sleep
- **LQS** (Local Queue Size) — Chase-Lev deque, victim with largest queue, fixed 50µs idle sleep
- **AS** (Adaptive Sleep) — Chase-Lev deque, random victim, p99-driven adaptive sleep (10–100µs)

## Environment (Benchmarked On)

| Component | Detail |
|-----------|--------|
| CPU | Intel Core i5-8350U @ 1.70 GHz (4C/8T) |
| OS | Arch Linux, kernel 6.x |
| Compiler | gcc 16.1.1 |
| Wrk | wrk 4.2.0 [epoll] |
| Python | 3.x |
| Python deps | numpy, scipy, matplotlib |

## Compilation

```bash
# Debug build (default)
make

# Release build (O3 + LTO + march=native)
make release

# Clean
make clean

# With AddressSanitizer
make CFLAGS="-Iinclude -Wall -Wextra -std=c11 -g -fsanitize=address -fsanitize=undefined"
```

The binary is produced at `bin/server`.

## Usage

```bash
./bin/server --help
```

```
Usage: bin/server [options]
  -p, --port <port>          Port number (default: 9000)
  -r, --root <path>          Document root (default: ./public)
  -t, --threads <n>          Number of worker threads (default: 4)
  -s, --scheduler <type>     Scheduler: rr, rs, lqs, as (default: rr)
  -b, --steal-batch <mode>   Steal batch: 1 or half (default: 1)
  -h, --help                 Show this help
```

### Quick Smoke Test

```bash
./bin/server -p 8080 -r ./corpus -t 4 -s rs &
curl -o /dev/null -w "%{http_code}" http://localhost:8080/   # → 200
kill %1
```

### Manual Benchmark

```bash
./bin/server -p 8080 -r ./corpus -t 4 -s as &
wrk -t4 -c32 -d30s --latency http://localhost:8080/
kill %1
```

## Automated Benchmark Suite

Two runners are provided:

### Python (recommended)

```bash
python3 run_bench.py
```

Runs 4 schedulers × 4 thread counts × 3 repeats = 48 runs. Results written to `bench_results/results.csv`. Uses `wrk -t4 -c32 -d10s`.
Each run requires 15 seconds. So, (48 * 15)/60 = 12 minutes are required for the script to complete.


### Analysis & Plots

```bash
python3 analyze.py
```

Reads `bench_results/results.csv` and produces 4 PNG plots in `bench_results/plots/`:
- `speedup.png` — throughput + speedup vs RR 1T across all schedulers
- `amdahl.png` — Amdahl's Law curve fit (estimates parallel fraction p)
- `4t_comparison.png` — bar chart at 4 threads
- `heatmap.png` — throughput heatmap (scheduler × threads)

## Results (10 s, wrk -t4 -c32, corpus/index.html)

| Scheduler | 1T | 2T | 4T | 8T | vs RR @ 8T |
|-----------|-----|-----|------|------|----------|
| RR | 23,240 | 25,902 | 10,853 | 8,159 | 1.00x |
| RS | 14,985 | 6,920 | 12,774 | 15,665 | 1.92x |
| LQS | 14,727 | 6,979 | 5,244 | 1,929 | 0.24x |
| AS | 15,303 | 7,607 | 15,451 | **16,648** | **2.04x** |

- **AS wins at 4–8 threads**: adaptive sleep reduces idle-wait overhead vs fixed-sleep RS
- **RR wins at 1–2 threads**: no steal overhead, but mutex contention kills scaling at 4T+
- **LQS loses**: O(n) scan of all deques per steal iteration is prohibitively expensive
- **2T dip** in all WS: all tasks land on worker 0, creating a steal bottleneck with few thieves
- Work-stealing schedulers (RS, AS) beat RR by 1.9–2.0× at 8 threads
