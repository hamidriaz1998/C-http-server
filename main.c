#include "include/queue.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
  Queue *queue = (Queue *)malloc(sizeof(Queue));
  queue_init(queue);
  int i;
  for (i = 0; i < 20; i += 1) {
    int *value = (int *)malloc(sizeof(int));
    *value = i;
    if (enqueue(queue, (void *)value)) {
      printf("Enqueued %d\n", *value);
    } else {
      printf("Enqueue failed: %d\n", *value);
    }
  }

  while (!queue_isempty(queue)) {
    void *value = dequeue(queue);
    if (value == NULL) {
      break;
    }
    int *i_ptr = (int *)value;
    printf("Dequeued %d\n", *i_ptr);
    free(i_ptr);
  }

  queue_free(queue);
  return 0;
}
