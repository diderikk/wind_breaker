#include "worker_queue.h"
#include "utils/logger.h"

int queue_init(queue_t *q) {
  q->front = 0;
  q->rear = 0;
  q->count = 0;

  if (pthread_mutex_init(&q->mutex, NULL) != 0) {
    log_error(__FILE__, "Could not initialize mutex");
    return -1;
  }
  if (pthread_cond_init(&q->cond, NULL) != 0) {
    log_error(__FILE__, "Could not initialize cond");
    return -1;
  }

  log_info(__FILE__, "Initialized queue with size %d", QUEUE_MAX_SIZE);

  return 0;
}

int queue_destroy(queue_t *q) {
  if (pthread_mutex_destroy(&q->mutex) != 0) {
    log_error(__FILE__, "Could not destroy mutex");
    return -1;
  }
  if (pthread_cond_destroy(&q->cond)) {
    log_error(__FILE__, "Could not destroy cond");
    return -1;
  }

  return 0;
}

void queue_push(queue_t *q, void *data) {
  pthread_mutex_lock(&q->mutex);

  while (q->count == QUEUE_MAX_SIZE) {
    // Wait until there is space in the queue
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  q->data[q->rear] = data;
  q->rear = (q->rear + 1) % QUEUE_MAX_SIZE;
  q->count++;

  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);
}

void *queue_pop(queue_t *q) {
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
  return data;
}
