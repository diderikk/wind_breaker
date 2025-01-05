#ifndef WORKER_H
#define WORKER_H

typedef struct {
  void *arg;
} worker_arg;

int init_workers(int worker_size, void *(*worker_func)(void *),
                 void *worker_func_arg);
int close_workers(int worker_size);

#endif // WORKER_H
