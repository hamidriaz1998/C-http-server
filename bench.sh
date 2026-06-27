#!/bin/bash
# Benchmark runner for the work-stealing HTTP server
# Usage: ./bench.sh [--schedulers "rr rs lqs as"] [--threads "1 2 4 8"] [--port 9000] [--duration 30s] [--warmup 10s]

HOST="http://localhost"
DURATION="30s"
WARMUP="10s"
SCHEDULERS="rr rs lqs as"
THREADS="1 2 4 8"
PORT=9000
WRK2="wrk"
REPEAT=3
RESULTS_DIR="bench_results"

usage() {
    echo "Usage: $0 [options]"
    echo "  --schedulers <list>    Space-separated scheduler list (default: '$SCHEDULERS')"
    echo "  --threads <list>       Space-separated thread counts (default: '$THREADS')"
    echo "  --port <n>             Base port (default: $PORT)"
    echo "  --duration <t>         Test duration (default: $DURATION)"
    echo "  --repeat <n>           Repeats per config (default: $REPEAT)"
    echo "  --wrk2 <path>          Path to wrk2 binary (default: $WRK2)"
    echo "  --help                 Show this help"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --schedulers) SCHEDULERS="$2"; shift 2 ;;
        --threads)    THREADS="$2"; shift 2 ;;
        --port)       PORT="$2"; shift 2 ;;
        --duration)   DURATION="$2"; shift 2 ;;
        --repeat)     REPEAT="$3"; shift 2 ;;
        --wrk2)       WRK2="$2"; shift 2 ;;
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
echo "scheduler,threads,run,throughput,p50,p95,p99,p999,errors" > "$RESULTS_FILE"

build_server() {
    make release 2>&1 | tail -1
}

run=0
total_runs=$(echo "$SCHEDULERS" | wc -w)
total_runs=$((total_runs * $(echo "$THREADS" | wc -w) * REPEAT))

for sched in $SCHEDULERS; do
    for thr in $THREADS; do
        for rep in $(seq 1 $REPEAT); do
            run=$((run + 1))
            port=$((PORT + run - 1))
            echo "[$run/$total_runs] sched=$sched threads=$thr run=$rep (port $port)"

            timeout $((DURATION + 5)) ./bin/server \
                --port "$port" \
                --threads "$thr" \
                --scheduler "$sched" > /dev/null 2>&1 &
            SERVER_PID=$!

            sleep 1

            if ! kill -0 $SERVER_PID 2>/dev/null; then
                echo "  Server failed to start"
                continue
            fi

            if [ "$WARMUP" != "0s" ]; then
                $WRK2 -t2 -c10 -d"$WARMUP" "http://localhost:$port/" > /dev/null 2>&1
            fi

            OUT=$($WRK2 -t2 -c100 -d"$DURATION" "http://localhost:$port/" 2>&1)
            kill $SERVER_PID 2>/dev/null
            wait $SERVER_PID 2>/dev/null

            throughput=$(echo "$OUT" | grep "Requests/sec" | awk '{print $2}')
            p50=$(echo "$OUT" | grep "50%" | awk '{print $2}')
            p95=$(echo "$OUT" | grep "95%" | awk '{print $2}')
            p99=$(echo "$OUT" | grep "99%" | awk '{print $2}')
            p999=$(echo "$OUT" | grep "99.9%" | awk '{print $2}' || echo "N/A")
            errors=$(echo "$OUT" | grep "Socket errors" | sed 's/.*Socket errors: //' || echo "0")

            echo "$sched,$thr,$rep,$throughput,$p50,$p95,$p99,$p999,$errors" >> "$RESULTS_FILE"
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
