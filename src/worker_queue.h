#ifndef QUEUE_H
#define QUEUE_H

#define QUEUE_MAX_SIZE 1024

#include "pthread.h"

typedef struct {
  void *data[QUEUE_MAX_SIZE];
  int front;
  int rear;
  int count;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} queue_t;

int queue_init(queue_t *q);
int queue_destroy(queue_t *q);
void queue_push(queue_t *q, void *data);
void *queue_pop(queue_t *q);

#endif // QUEUE_H
