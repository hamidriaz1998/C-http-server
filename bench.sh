#!/bin/bash
# Benchmark runner for the work-stealing HTTP server
set -euo pipefail

DURATION=15
WARMUP=5
SCHEDULERS="rr rs lqs as"
THREADS="1 2 4 8"
PORT=9000
WRK="wrk"
REPEAT=3
RESULTS_DIR="bench_results"
ROOT="./corpus"

usage() {
    cat <<EOF
Usage: $0 [options]
  --schedulers <list>    Space-separated scheduler list (default: '$SCHEDULERS')
  --threads <list>       Space-separated thread counts (default: '$THREADS')
  --port <n>             Base port (default: $PORT)
  --duration <s>         Test duration in seconds (default: $DURATION)
  --warmup <s>           Warmup duration in seconds (default: $WARMUP)
  --repeat <n>           Repeats per config (default: $REPEAT)
  --root <path>          Document root (default: $ROOT)
  --help                 Show this help
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --schedulers) SCHEDULERS="$2"; shift 2 ;;
        --threads)    THREADS="$2"; shift 2 ;;
        --port)       PORT="$2"; shift 2 ;;
        --duration)   DURATION="$2"; shift 2 ;;
        --warmup)     WARMUP="$2"; shift 2 ;;
        --repeat)     REPEAT="$2"; shift 2 ;;
        --root)       ROOT="$2"; shift 2 ;;
        --help)       usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

mkdir -p "$RESULTS_DIR"

echo "=== System Info ===" > "$RESULTS_DIR/system.info"
uname -a >> "$RESULTS_DIR/system.info"
grep "model name" /proc/cpuinfo | head -1 >> "$RESULTS_DIR/system.info"
grep "cpu cores" /proc/cpuinfo | head -1 >> "$RESULTS_DIR/system.info"
grep "^processor" /proc/cpuinfo | wc -l | xargs echo "logical cpus:" >> "$RESULTS_DIR/system.info"
ulimit -n >> "$RESULTS_DIR/system.info"
cat /proc/sys/net/core/somaxconn >> "$RESULTS_DIR/system.info"
echo "" >> "$RESULTS_DIR/system.info"

RESULTS_FILE="$RESULTS_DIR/results.csv"
echo "scheduler,threads,run,throughput,p50_ms,p75_ms,p99_ms,errors" > "$RESULTS_FILE"

run=0
total_runs=$(echo "$SCHEDULERS" | wc -w)
total_runs=$((total_runs * $(echo "$THREADS" | wc -w) * REPEAT))

for sched in $SCHEDULERS; do
    for thr in $THREADS; do
        for rep in $(seq 1 $REPEAT); do
            run=$((run + 1))
            port=$((PORT + run))
            echo "[$run/$total_runs] sched=$sched threads=$thr run=$rep"

            timeout $((DURATION + WARMUP + 5)) ./bin/server \
                --port "$port" \
                --threads "$thr" \
                --scheduler "$sched" \
                --root "$ROOT" > "$RESULTS_DIR/server_${sched}_${thr}_${rep}.log" 2>&1 &
            SERVER_PID=$!

            sleep 1

            if ! kill -0 $SERVER_PID 2>/dev/null; then
                echo "  Server failed to start"
                continue
            fi

            if [ "$WARMUP" -gt 0 ]; then
                $WRK -t2 -c10 -d"${WARMUP}s" "http://localhost:$port/" > /dev/null 2>&1 || true
            fi

            OUT=$($WRK -t2 -c100 -d"${DURATION}s" "http://localhost:$port/" 2>&1 || true)

            kill $SERVER_PID 2>/dev/null || true
            wait $SERVER_PID 2>/dev/null || true

            throughput=$(echo "$OUT" | grep "Requests/sec" | awk '{print $2}')
            p50=$(echo "$OUT" | grep "50.000%" | awk '{print $2}')
            p75=$(echo "$OUT" | grep "75.000%" | awk '{print $2}')
            p99=$(echo "$OUT" | grep "99.000%" | awk '{print $2}')
            errors=$(echo "$OUT" | grep "Socket errors" | sed 's/.*Socket errors: //' || echo "0")

            echo "$sched,$thr,$rep,$throughput,$p50,$p75,$p99,$errors" >> "$RESULTS_FILE"
            echo "  → $throughput req/s, p50=$p50, p99=$p99"
        done
    done
done

echo ""
echo "=== Results ==="
cat "$RESULTS_FILE" | column -t -s','
echo ""
echo "Full results saved to $RESULTS_FILE"
echo "System info saved to $RESULTS_DIR/system.info"
