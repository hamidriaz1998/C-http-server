#ifndef DEQUE_H
#define DEQUE_H

#include <stdatomic.h>
#include <stddef.h>

typedef struct task task_t;

typedef struct {
    task_t **buffer;
    atomic_int size;
    atomic_int top;
    atomic_int bottom;
} deque_t;

deque_t *deque_init(void);
void deque_push_bottom(deque_t *d, task_t *t);
task_t *deque_pop_bottom(deque_t *d);
task_t *deque_steal(deque_t *d);
int deque_size(deque_t *d);
void deque_free(deque_t *d);

#endif
