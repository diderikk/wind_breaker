#include "worker.h"
#include "utils/logger.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int workers_init(pthread_t *workers[], int worker_size,
                 void *(*worker_func)(void *), void *worker_func_arg) {

  for (int i = 0; i < worker_size; i++) {
    pthread_t thread;
    worker_arg *arg = (worker_arg *)malloc(sizeof(worker_arg));

    arg->arg = worker_func_arg;
    if (pthread_create(&thread, NULL, worker_func, arg) != 0) {
      log_error("Error occured initializing thread");
      return -1;
    }
  }

  log_info("Initialized %d workers ready to handle requests", worker_size);

  return 0;
}

int workers_close(pthread_t *workers[], int *worker_size) {
  int closed_threads = 0;
  for (int i = 0; i < *worker_size; i++) {
    // Request thread cancellation
    //    if (pthread_cancel(*threads[i]) != 0) {
    //        perror("Failed to cancel thread");
    //        return -1;
    //    }

    // Wait for the thread to exit
    if (pthread_join(*workers[i], NULL) != 0) {
      log_error("Failed to join thread");
      return -1;
    }
  }
  return closed_threads;
}
