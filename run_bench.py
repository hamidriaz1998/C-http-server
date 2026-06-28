#!/usr/bin/env python3
"""Benchmark runner for the work-stealing HTTP server."""
import csv, os, re, signal, subprocess, sys, time

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
    cpu = subprocess.run(["grep", "model name", "/proc/cpuinfo"], capture_output=True, text=True).stdout
    cores = subprocess.run(["grep", "^processor", "/proc/cpuinfo"], capture_output=True, text=True).stdout.count("\n")
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

            proc = subprocess.Popen(
                [BIN, "--port", str(port), "--threads", str(thr),
                 "--scheduler", sched, "--root", "./corpus"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(1)

            if proc.poll() is not None:
                print("  Server failed to start")
                continue

            try:
                subprocess.run([WRK, f"-t{WRK_THREADS}", "-c10", "-d" + WARMUP,
                                f"http://localhost:{port}/"],
                               capture_output=True, timeout=10)

                out = subprocess.run([WRK, f"-t{WRK_THREADS}", f"-c{WRK_CONNS}", "-d" + DURATION,
                                      f"http://localhost:{port}/"],
                                     capture_output=True, text=True, timeout=30)
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
            except:
                proc.kill()
                proc.wait()

            def extract(pattern, default="N/A"):
                m = re.search(pattern, output)
                return m.group(1).strip() if m else default

            throughput = extract(r"Requests/sec:\s+([\d.]+)")
            p50 = extract(r"50\.000%\s+([\d.]+(?:ms|us|s))")
            p75 = extract(r"75\.000%\s+([\d.]+(?:ms|us|s))")
            p99 = extract(r"99\.000%\s+([\d.]+(?:ms|us|s))")
            errors = extract(r"Socket errors:\s+(.+)", "0")

            results.append({
                "scheduler": sched, "threads": thr, "run": rep,
                "throughput": throughput, "p50": p50, "p75": p75, "p99": p99, "errors": errors
            })
            print(f"  → {throughput} req/s, p50={p50}, p99={p99}")

with open(f"{RESULTS_DIR}/results.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["scheduler","threads","run","throughput","p50","p75","p99","errors"])
    w.writeheader()
    w.writerows(results)

print(f"\nDone. Results saved to {RESULTS_DIR}/results.csv")
