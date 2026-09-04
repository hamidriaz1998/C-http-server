#include "../include/metrics.h"
#include <stdio.h>
#include <time.h>

void metrics_init(worker_metrics_t *m) {
    atomic_init(&m->tasks_completed, 0);
    atomic_init(&m->steal_attempts, 0);
    atomic_init(&m->steal_successes, 0);
    atomic_init(&m->idle_ns, 0);
    atomic_init(&m->task_ns_sum, 0);
    atomic_init(&m->task_ns_max, 0);
    for (int i = 0; i < HISTO_BUCKETS; i++)
        atomic_init(&m->latency_histo[i], 0);
}

void metrics_record_task(worker_metrics_t *m, uint64_t start_ns, uint64_t end_ns) {
    uint64_t dur = end_ns - start_ns;
    atomic_fetch_add(&m->tasks_completed, 1);
    atomic_fetch_add(&m->task_ns_sum, dur);
    uint64_t prev_max = atomic_load(&m->task_ns_max);
    while (dur > prev_max) {
        if (atomic_compare_exchange_weak(&m->task_ns_max, &prev_max, dur))
            break;
    }

    int bucket = (dur == 0) ? 0 : (64 - __builtin_clzll(dur));
    if (bucket < 0) bucket = 0;
    if (bucket >= HISTO_BUCKETS) bucket = HISTO_BUCKETS - 1;
    atomic_fetch_add(&m->latency_histo[bucket], 1);
}

void metrics_record_steal(worker_metrics_t *m, int success) {
    atomic_fetch_add(&m->steal_attempts, 1);
    if (success)
        atomic_fetch_add(&m->steal_successes, 1);
}

uint64_t metrics_percentile(worker_metrics_t *m, double p) {
    uint64_t total = atomic_load(&m->tasks_completed);
    if (total == 0) return 0;

    uint64_t target = (uint64_t)(total * p / 100.0);
    if (target == 0) target = 1;

    uint64_t cum = 0;
    for (int i = 0; i < HISTO_BUCKETS; i++) {
        cum += atomic_load(&m->latency_histo[i]);
        if (cum >= target)
            return (i == 0) ? 0 : (1ULL << i);
    }
    return (1ULL << (HISTO_BUCKETS - 1));
}
