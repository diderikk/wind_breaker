#ifndef WORKER_H
#define WORKER_H

#include "static.h"
#include <signal.h>

int init_workers(int worker_size, void *(*worker_func)(void *),
                 void *worker_func_arg);
int close_workers(int worker_size);

#endif // WORKER_H
