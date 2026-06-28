#!/usr/bin/env python3
"""Analyze benchmark results and generate plots."""
import csv
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.optimize import curve_fit

RESULTS_DIR = "bench_results"
PLOTS_DIR = os.path.join(RESULTS_DIR, "plots")
os.makedirs(PLOTS_DIR, exist_ok=True)

rows = []
with open(os.path.join(RESULTS_DIR, "results.csv")) as f:
    reader = csv.DictReader(f)
    for r in reader:
        r["throughput"] = float(r["throughput"])
        rows.append(r)

# Aggregate by scheduler + threads
from collections import defaultdict
agg = defaultdict(list)
for r in rows:
    key = (r["scheduler"], int(r["threads"]))
    agg[key].append(r["throughput"])

schedulers = ["rr", "rs", "lqs", "as"]
colors = {"rr": "#1f77b4", "rs": "#ff7f0e", "lqs": "#2ca02c", "as": "#d62728"}
markers = {"rr": "o", "rs": "s", "lqs": "^", "as": "D"}
threads = sorted(set(k[1] for k in agg))

# --- Speedup plot ---
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

for sched in schedulers:
    vals = []
    errs = []
    for t in threads:
        data = agg[(sched, t)]
        vals.append(np.mean(data))
        errs.append(np.std(data))
    ax1.errorbar(threads, vals, yerr=errs, label=sched.upper(),
                 color=colors[sched], marker=markers[sched], capsize=3)

ax1.set_xlabel("Worker Threads")
ax1.set_ylabel("Throughput (req/s)")
ax1.set_title("Throughput vs Thread Count")
ax1.legend()
ax1.grid(True, alpha=0.3)

# --- Speedup relative to RR 1T ---
rr1t_mean = np.mean(agg[("rr", 1)])
for sched in schedulers:
    vals = []
    for t in threads:
        data = agg[(sched, t)]
        speedup = np.mean(data) / rr1t_mean
        vals.append(speedup)
    ax2.plot(threads, vals, label=sched.upper(),
             color=colors[sched], marker=markers[sched])

ax2.axhline(y=1, color="gray", linestyle="--", alpha=0.5)
ax2.set_xlabel("Worker Threads")
ax2.set_ylabel("Speedup (relative to RR 1T)")
ax2.set_title("Speedup vs RR 1-Thread Baseline")
ax2.legend()
ax2.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(os.path.join(PLOTS_DIR, "speedup.png"), dpi=150)
print(f"Saved speedup.png")

# --- Amdahl's Law fit ---
fig, ax = plt.subplots(figsize=(8, 5))

for sched in schedulers:
    vals = []
    for t in threads:
        data = agg[(sched, t)]
        vals.append(np.mean(data))

    def amdahl(n, p, s):
        return s / (1 - p + p / n)

    try:
        popt, _ = curve_fit(lambda n, p: amdahl(n, p, vals[0]),
                           np.array(threads, dtype=float),
                           np.array(vals, dtype=float),
                           p0=[0.9])
        p_est = popt[0]
        label = f"{sched.upper()} (p={p_est:.3f})"
        fit = [amdahl(t, p_est, vals[0]) for t in threads]
        ax.plot(threads, vals, color=colors[sched], marker=markers[sched],
                linestyle="none")
        ax.plot(threads, fit, color=colors[sched], linestyle="--",
                label=label, alpha=0.7)
    except Exception as e:
        print(f"  Amdahl fit failed for {sched}: {e}")

ax.set_xlabel("Worker Threads")
ax.set_ylabel("Throughput (req/s)")
ax.set_title("Amdahl's Law Fit")
ax.legend()
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(os.path.join(PLOTS_DIR, "amdahl.png"), dpi=150)
print(f"Saved amdahl.png")

# --- Steal overhead / breakdown for 4T ---
fig, ax = plt.subplots(figsize=(8, 5))

t4_results = [r for r in rows if int(r["threads"]) == 4]
for sched in schedulers:
    sched_runs = [r for r in t4_results if r["scheduler"] == sched]
    throughputs = [r["throughput"] for r in sched_runs]
    mean_tp = np.mean(throughputs)
    ax.bar(sched.upper(), mean_tp, color=colors[sched], alpha=0.8)

ax.set_ylabel("Throughput (req/s)")
ax.set_title("Throughput at 4 Worker Threads")
ax.grid(True, alpha=0.3, axis="y")

plt.tight_layout()
plt.savefig(os.path.join(PLOTS_DIR, "4t_comparison.png"), dpi=150)
print(f"Saved 4t_comparison.png")

# --- Scalability heatmap ---
fig, ax = plt.subplots(figsize=(9, 4))

data_matrix = []
for sched in schedulers:
    row = []
    for t in threads:
        row.append(np.mean(agg[(sched, t)]))
    data_matrix.append(row)

im = ax.imshow(data_matrix, cmap="viridis", aspect="auto")
ax.set_xticks(range(len(threads)))
ax.set_xticklabels([f"{t}T" for t in threads])
ax.set_yticks(range(len(schedulers)))
ax.set_yticklabels([s.upper() for s in schedulers])
for i in range(len(schedulers)):
    for j in range(len(threads)):
        ax.text(j, i, f"{data_matrix[i][j]:.0f}",
                ha="center", va="center", color="white" if data_matrix[i][j] < np.mean(data_matrix) else "black",
                fontsize=9)
ax.set_title("Throughput Heatmap (req/s)")
plt.colorbar(im, ax=ax, label="req/s")
plt.tight_layout()
plt.savefig(os.path.join(PLOTS_DIR, "heatmap.png"), dpi=150)
print(f"Saved heatmap.png")

# --- Print summary table ---
print("\n=== Summary Table ===")
print(f"{'Scheduler':>8} | {'1T':>8} | {'2T':>8} | {'4T':>8} | {'8T':>8}")
print("-" * 48)
for sched in schedulers:
    vals = [f"{np.mean(agg[(sched, t)]):8.0f}" for t in threads]
    print(f"{sched.upper():>8} | " + " | ".join(vals))

# Relative to RR
print("\n=== Speedup vs RR ===")
for sched in schedulers:
    vals = []
    for t in threads:
        rr_val = np.mean(agg[("rr", t)])
        s_val = np.mean(agg[(sched, t)])
        vals.append(f"{s_val / rr_val:7.2f}x")
    print(f"{sched.upper():>8} | " + " | ".join(vals))

print(f"\nPlots saved to {PLOTS_DIR}/")
