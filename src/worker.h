#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

typedef struct {
  void *arg;
} worker_arg;

int workers_init(int worker_size, void *(*worker_func)(void *),
                 void *worker_func_arg);
int workers_close(int *worker_size);

#endif // WORKER_H
