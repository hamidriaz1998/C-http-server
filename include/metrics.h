#ifndef METRICS_H
#define METRICS_H

#include <stdatomic.h>
#include <stdint.h>

#define LATENCY_BUCKETS 64

typedef struct {
    atomic_uint_least64_t tasks_completed;
    atomic_uint_least64_t steal_attempts;
    atomic_uint_least64_t steal_successes;
    atomic_uint_least64_t idle_ns;
    atomic_uint_least64_t task_ns_sum;
    atomic_uint_least64_t task_ns_max;
} worker_metrics_t;

void metrics_init(worker_metrics_t *m);
void metrics_record_task(worker_metrics_t *m, uint64_t start_ns, uint64_t end_ns);
void metrics_record_steal(worker_metrics_t *m, int success);

#endif
