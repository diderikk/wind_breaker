#include "worker.h"
#include "data_structures/session.h"
#include "handlers/builder.c"
#include "handlers/loader.c"
#include "handlers/parser.c"
#include "handlers/sender.c"
#include "utils/logger.h"
#include <pthread.h>
#include <stdlib.h>

static pthread_t *workers = NULL;

static void *(*handlers[])() = {
    &handle_a,
    &handle_b,
    &handle_c,
    &handle_d,
};
static unsigned int handlers_size = 4;

int init_workers(int worker_size, void *worker_func_arg) {
  assert(worker_size > 0);
  assert(workers == NULL);

  workers =
      (pthread_t *)malloc((sizeof(pthread_t) * worker_size) * handlers_size);

  int prev_index = -1;
  for (int i = 0; i < handlers_size; i++) {
    for (int j = 0; j < worker_size; j++) {
      int index = i * worker_size + j;
      assert(index != prev_index);
      // worker_arg *arg = (worker_arg *)malloc(sizeof(worker_arg));
      // arg->arg = worker_func_arg;

      assert(pthread_create(&workers[index], NULL, handlers[i], NULL) == 0);
      prev_index = index;
    }
  }

  log_info("Initialized %d workers ready to handle requests", worker_size);

  return 0;
}

int close_workers(int worker_size) {
  assert(workers != NULL);

  broadcast_session();
  int prev_index = -1;
  for (int i = 0; i < handlers_size; i++) {
    for (int j = 0; j < worker_size; j++) {
      int index = i * worker_size + j;
      assert(index != prev_index);
      // Wait for the thread to exit
      assert(pthread_join(workers[index], NULL) == 0);
      prev_index = index;
    }
  }
  free(workers);
  workers = NULL;
  return 0;
}
