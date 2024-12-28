#include "worker_queue.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include <stdlib.h>
#include <string.h>

static queue_t *q = NULL;

int init_queue() {
  assert(QUEUE_MAX_SIZE > 0);
  assert(q == NULL);

  q = (queue_t *)malloc(sizeof(queue_t));
  q->data = (request_data **)malloc(sizeof(request_data) * QUEUE_MAX_SIZE);
  for (int i = 0; i < QUEUE_MAX_SIZE; i++) {
    q->data[i] = (request_data *)malloc(sizeof(request_data));
    q->data[i]->fd = 0;
    memset(q->data[i]->data, 0, HTTP_BODY_SIZE);
  }

  q->front = 0;
  q->rear = 0;
  q->count = 0;

  assert(pthread_mutex_init(&q->mutex, NULL) == 0);
  assert(pthread_cond_init(&q->cond, NULL) == 0);

  log_info("Initialized queue with size %d", QUEUE_MAX_SIZE);
  return 0;
}

int destroy_queue() {
  assert(q != NULL);
  assert(pthread_mutex_destroy(&q->mutex) == 0);
  assert(pthread_cond_destroy(&q->cond) == 0);

  free(q);
  q = NULL;

  log_info("Destroyed queue");
  return 0;
}

void queue_push(request_data *data) {
  assert(q != NULL);
  assert(data != NULL);
  pthread_mutex_lock(&q->mutex);

  while (q->count == QUEUE_MAX_SIZE) {
    // Wait until there is space in the queue
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  memcpy(q->data[q->rear], data, sizeof(request_data));
  q->rear = (q->rear + 1) % QUEUE_MAX_SIZE;
  q->count++;

  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);

  log_trace("Pushed data to queue, count: %d", q->count);
}

request_data *queue_pop() {
  assert(q != NULL);
  pthread_mutex_lock(&q->mutex);

  while (q->count == 0) {
    // Wait until there is data in the queue
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  void *data = q->data[q->front];
  q->front = (q->front + 1) % QUEUE_MAX_SIZE;
  q->count--;

  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);

  assert(data != NULL);
  log_trace("Popped data from queue, count: %d", q->count);
  return data;
}
