#include "cases.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include "connection_cases.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

static const int case_count = 2;
static pthread_t *cases = NULL;

int start_case(void *(*func)(void *), struct connection_data *arg, int case_id);

void run_cases(char *ip, char *port) {
  long seed = time(NULL);
  log_info("Running cases with seed %ld", seed);
  cases = (pthread_t *)malloc(sizeof(pthread_t) * case_count);
  // Validate server is reachable
  assert(test_connection(ip, port) == 0);

  struct connection_data arg = {ip, port, seed};
  start_case(start_connection_delay_before_close, &arg, 0);
  start_case(start_connections_simultaneously, &arg, 1);

  for (int i = 0; i < case_count; i++) {
    pthread_join(cases[i], NULL);
  }
}

int start_case(void *(*func)(void *), struct connection_data *arg,
               int case_id) {
  assert(cases != NULL);
  assert(case_id < case_count);
  log_trace("Starting case %d", case_id);
  log_debug("arg: %s:%s, seed: %ld", arg->ip, arg->port, arg->seed);

  assert(pthread_create(&cases[case_id], NULL, func, arg) == 0);

  return 0;
}
