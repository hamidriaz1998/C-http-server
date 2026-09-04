#!/usr/bin/env python3
"""Benchmark runner for the work-stealing HTTP server."""

import csv
import os
import re
import subprocess
import time

BIN = "./bin/server"
WRK = "wrk"
RESULTS_DIR = "bench_results"
DURATION = "10s"
WARMUP = "3s"
WRK_THREADS = 4
WRK_CONNS = 32
SCHEDULERS = ["rr", "rs", "lqs", "as"]
THREADS = [1, 2, 4, 8]
REPEAT = 3
BASE_PORT = 9200

os.makedirs(RESULTS_DIR, exist_ok=True)

with open(f"{RESULTS_DIR}/system.info", "w") as f:
    f.write(subprocess.run(["uname", "-a"], capture_output=True, text=True).stdout)
    cpu = subprocess.run(
        ["grep", "model name", "/proc/cpuinfo"], capture_output=True, text=True
    ).stdout
    cores = subprocess.run(
        ["grep", "^processor", "/proc/cpuinfo"], capture_output=True, text=True
    ).stdout.count("\n")
    f.write(cpu.split("\n")[0] + "\n")
    f.write(f"logical cpus: {cores}\n")

results = []
total = len(SCHEDULERS) * len(THREADS) * REPEAT
run_idx = 0

for sched in SCHEDULERS:
    for thr in THREADS:
        for rep in range(1, REPEAT + 1):
            run_idx += 1
            port = BASE_PORT + run_idx
            print(f"[{run_idx}/{total}] sched={sched} threads={thr} rep={rep}")

            log_file = f"{RESULTS_DIR}/server_{sched}_{thr}_{rep}.log"
            with open(log_file, "w") as logf:
                proc = subprocess.Popen(
                    [
                        BIN,
                        "--port",
                        str(port),
                        "--threads",
                        str(thr),
                        "--scheduler",
                        sched,
                        "--root",
                        "./corpus",
                    ],
                    stdout=logf,
                    stderr=subprocess.STDOUT,
                )
            time.sleep(1)

            if proc.poll() is not None:
                print("  Server failed to start")
                continue

            try:
                subprocess.run(
                    [
                        WRK,
                        f"-t{WRK_THREADS}",
                        "-c10",
                        "-d" + WARMUP,
                        f"http://localhost:{port}/",
                    ],
                    capture_output=True,
                    timeout=10,
                )

                out = subprocess.run(
                    [
                        WRK,
                        f"-t{WRK_THREADS}",
                        f"-c{WRK_CONNS}",
                        "-d" + DURATION,
                        f"http://localhost:{port}/",
                    ],
                    capture_output=True,
                    text=True,
                    timeout=30,
                )
                output = out.stdout + out.stderr
            except subprocess.TimeoutExpired:
                print("  wrk timed out")
                output = ""
            except Exception as e:
                print(f"  wrk error: {e}")
                output = ""

            proc.terminate()
            try:
                proc.wait(timeout=3)
            except Exception:
                proc.kill()
                proc.wait()

            def extract(pattern, default="N/A"):
                m = re.search(pattern, output)
                return m.group(1).strip() if m else default

            throughput = extract(r"Requests/sec:\s+([\d.]+)")
            errors = extract(r"Socket errors:\s+(.+)", "0")

            def parse_lat(name):
                content = open(log_file).read()
                m_global = re.search(
                    r"=== Global Latencies ===(.*?)(?:\n===|\Z)", content, re.DOTALL
                )
                block = m_global.group(1) if m_global else content
                pat = rf"{name}=(\d+)"
                m2 = re.search(pat, block)
                return m2.group(1) if m2 else "N/A"

            lat_p50 = parse_lat("p50")
            lat_p95 = parse_lat("p95")
            lat_p99 = parse_lat("p99")
            lat_p999 = parse_lat("p999")

            results.append(
                {
                    "scheduler": sched,
                    "threads": thr,
                    "run": rep,
                    "throughput": throughput,
                    "lat_p50": lat_p50,
                    "lat_p95": lat_p95,
                    "lat_p99": lat_p99,
                    "lat_p999": lat_p999,
                    "errors": errors,
                }
            )
            print(f"  → {throughput} req/s, p50={lat_p50}us, p99={lat_p99}us")

with open(f"{RESULTS_DIR}/results.csv", "w", newline="") as f:
    w = csv.DictWriter(
        f,
        fieldnames=[
            "scheduler",
            "threads",
            "run",
            "throughput",
            "lat_p50",
            "lat_p95",
            "lat_p99",
            "lat_p999",
            "errors",
        ],
    )
    w.writeheader()
    w.writerows(results)

print(f"\nDone. Results saved to {RESULTS_DIR}/results.csv")
