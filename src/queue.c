#include "../include/queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void queue_init(Queue *q) {
  if (q != NULL) {
    return;
  }
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;
}

bool enqueue(Queue *q, void *data) {
  queue_node_t *new_node = (queue_node_t *)malloc(sizeof(queue_node_t));
  if (new_node == NULL) {
    return false;
  }

  new_node->data = data;
  new_node->next = NULL;

  if (q->tail == NULL) {
    q->head = new_node;
  } else {
    q->tail->next = new_node;
  }
  q->tail = new_node;
  q->size++;
  return true;
}

void *dequeue(Queue *q) {
  if (queue_isempty(q)) {
    return NULL;
  }

  queue_node_t *head = q->head;
  q->head = head->next;
  q->size--;

  void *data = head->data;

  // Free QueueNode
  free(head);

  return data;
}

bool queue_isempty(Queue *q) { return q->head == NULL; }

void queue_free(Queue *q) {
  if (queue_isempty(q)) {
    return;
  }
  queue_node_t *current = q->head;
  while (current != NULL) {
    queue_node_t *next = current->next;
    free(current);
    current = next;
  }
  free(q);
}
