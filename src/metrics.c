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
}

void metrics_record_steal(worker_metrics_t *m, int success) {
    atomic_fetch_add(&m->steal_attempts, 1);
    if (success)
        atomic_fetch_add(&m->steal_successes, 1);
}
