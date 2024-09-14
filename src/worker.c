#include "worker.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int workers_init(pthread_t *workers[], int worker_size,
                 void *(*worker_func)(void *), void *worker_func_arg) {

  for (int i = 0; i < worker_size; i++) {
    pthread_t thread;
    worker_arg *arg = (worker_arg *)malloc(sizeof(worker_arg));

    arg->arg = worker_func_arg;
    arg->worker_id = i;
    if (pthread_create(&thread, NULL, worker_func, arg) != 0) {
      perror("Error occured initializing thread");
      return -1;
    }
  }

  printf("Initialized %d workers ready to handle requests\n", worker_size);

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
      perror("Failed to join thread");
      return -1;
    }
  }
  return closed_threads;
}
