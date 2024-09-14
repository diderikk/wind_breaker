#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

typedef struct {
  void *arg;
  int worker_id;
} worker_arg;

int workers_init(pthread_t *workers[], int worker_size,
                 void *(*worker_func)(void *), void *worker_func_arg);
int workers_close(pthread_t *workers[], int *worker_size);

#endif // WORKER_H
