#include "worker.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include <pthread.h>
#include <stdlib.h>

static pthread_t *workers = NULL;

int init_workers(int worker_size, void *(*worker_func)(void *),
                 void *worker_func_arg) {
  assert(worker_size > 0);
  assert(workers == NULL);
  assert(worker_func != NULL);

  workers = (pthread_t *)malloc(sizeof(pthread_t) * worker_size);

  for (int i = 0; i < worker_size; i++) {
    pthread_t thread;
    worker_arg *arg = (worker_arg *)malloc(sizeof(worker_arg));

    arg->arg = worker_func_arg;
    assert(pthread_create(&thread, NULL, worker_func, arg) == 0);
  }

  log_info("Initialized %d workers ready to handle requests", worker_size);

  return 0;
}

int close_workers(int worker_size) {
  int closed_threads = 0;
  for (int i = 0; i < worker_size; i++) {
    // Wait for the thread to exit
    assert(pthread_join(workers[i], NULL) != 0);
  }
  free(workers);
  return closed_threads;
}
