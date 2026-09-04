#include "../include/deque.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

#define DEQUE_INIT_SIZE 8192

deque_t *deque_init(void) {
    deque_t *d = malloc(sizeof(deque_t));
    if (!d) return NULL;

    task_t **buf = calloc(DEQUE_INIT_SIZE, sizeof(task_t *));
    if (!buf) {
        free(d);
        return NULL;
    }
    atomic_init(&d->buffer, buf);
    atomic_init(&d->size, DEQUE_INIT_SIZE);
    atomic_init(&d->top, 0);
    atomic_init(&d->bottom, 0);
    atomic_init(&d->cas_fail_count, 0);
    return d;
}

static int deque_grow(deque_t *d, int new_bottom, int new_top) {
    int old_size = atomic_load(&d->size);
    int new_size = old_size * 2;
    task_t **new_buf = calloc(new_size, sizeof(task_t *));
    if (!new_buf) return -1;

    for (int i = new_top; i < new_bottom; i++)
        new_buf[i] = (task_t *)atomic_load(&d->buffer)[i];

    atomic_store(&d->buffer, new_buf);
    atomic_store(&d->size, new_size);
    return 0;
}

void deque_push_bottom(deque_t *d, task_t *t) {
    int b = atomic_load(&d->bottom);
    int size = atomic_load(&d->size);

    if (b >= size) {
        int top = atomic_load(&d->top);
        if (deque_grow(d, b, top) != 0)
            return;
    }

    task_t **buf = atomic_load(&d->buffer);
    buf[b] = t;
    atomic_thread_fence(memory_order_release);
    atomic_store(&d->bottom, b + 1);
}

task_t *deque_pop_bottom(deque_t *d) {
    int b = atomic_load(&d->bottom) - 1;
    atomic_store(&d->bottom, b);
    atomic_thread_fence(memory_order_seq_cst);
    int t = atomic_load(&d->top);

    task_t *task = NULL;
    if (t <= b) {
        task_t **buf = atomic_load(&d->buffer);
        task = buf[b];
        if (t == b) {
            if (!atomic_compare_exchange_strong(&d->top, &t, t + 1)) {
                task = NULL;
                atomic_fetch_add(&d->cas_fail_count, 1);
            }
            atomic_store(&d->bottom, b + 1);
        }
    } else {
        atomic_store(&d->bottom, b + 1);
    }
    return task;
}

task_t *deque_steal(deque_t *d) {
    int t = atomic_load(&d->top);
    atomic_thread_fence(memory_order_seq_cst);
    int b = atomic_load(&d->bottom);

    task_t *task = NULL;
    if (t < b) {
        task_t **buf = atomic_load(&d->buffer);
        task = buf[t];
        if (!atomic_compare_exchange_strong(&d->top, &t, t + 1)) {
            task = NULL;
            atomic_fetch_add(&d->cas_fail_count, 1);
        }
    }
    return task;
}

int deque_size(deque_t *d) {
    return atomic_load(&d->bottom) - atomic_load(&d->top);
}

void deque_free(deque_t *d) {
    if (!d) return;
    free(atomic_load(&d->buffer));
    free(d);
}
