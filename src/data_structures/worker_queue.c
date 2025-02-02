#include "worker_queue.h"
#include "../properties.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include <stdlib.h>
#include <string.h>

static queue_t *q = NULL;

int init_queue() {
  assert(get_queue_max_size() > 0);
  assert(q == NULL);

  q = (queue_t *)malloc(sizeof(queue_t));
  q->data = (worker_data **)calloc(get_queue_max_size(), sizeof(worker_data));
  for (int i = 0; i < get_queue_max_size(); i++) {
    q->data[i] = (worker_data *)calloc(1, sizeof(worker_data));
  }

  q->front = 0;
  q->rear = 0;
  q->count = 0;

  assert(pthread_mutex_init(&q->mutex, NULL) == 0);
  assert(pthread_cond_init(&q->cond, NULL) == 0);

  log_info("Initialized queue with size %d", get_queue_max_size());
  return 0;
}

void destroy_queue() {
  assert(q != NULL);

  for (int i = 0; i < get_queue_max_size(); i++) {
    if (q->data[i] != NULL) {
      free(q->data[i]);
      q->data[i] = NULL;
    }
  }
  free(q->data);
  q->data = NULL;

  assert(pthread_mutex_destroy(&q->mutex) == 0);
  assert(pthread_cond_destroy(&q->cond) == 0);

  free(q);
  q = NULL;
}

void broadcast_queue() {
  assert(q != NULL);
  pthread_mutex_lock(&q->mutex);
  pthread_cond_broadcast(&q->cond);
  pthread_mutex_unlock(&q->mutex);
}

void queue_push(int fd, const char *data) {
  assert(q != NULL);
  assert(data != NULL);
  pthread_mutex_lock(&q->mutex);

  while (q->count == get_queue_max_size()) {
    // Wait until there is space in the queue
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  q->data[q->rear]->fd = fd;
  memcpy(q->data[q->rear]->data, data, REQUEST_RESPONSE_MAX_SIZE);
  q->rear = (q->rear + 1) % get_queue_max_size();
  q->count++;

  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);

  log_trace("Pushed data to queue, count: %d", q->count);
}

worker_data *queue_pop() {
  assert(q != NULL);
  pthread_mutex_lock(&q->mutex);

  while (q->count == 0 && !stop()) {
    // Wait until there is data in the queue
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  if (stop()) {
    pthread_mutex_unlock(&q->mutex);
    return NULL;
  }

  void *data = q->data[q->front];
  q->front = (q->front + 1) % get_queue_max_size();
  q->count--;

  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);

  assert(data != NULL);
  log_trace("Popped data from queue, count: %d", q->count);
  return data;
}
