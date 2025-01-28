#include "../../src/utils/assert2.h"
#include "../../src/utils/logger.h"
#include "../../src/data_structures/session.h"
#include "../../src/properties.h"
#include <pthread.h>
#include <stdlib.h>


static int session_test_count = 0;
static pthread_t *session_test_threads = NULL;


static int session_start_case(void *(*func)(void *), const char *name);

void *init_session_test() {
  init_session_cache(3);

  return NULL;
}


int session_test() {
  session_test_threads = (pthread_t *)malloc(sizeof(pthread_t) * 100);

  session_start_case(init_session_test, "init_session_test");

  for (int i = 0; i < session_test_count; i++) {
    pthread_join(session_test_threads[i], NULL);
  }
  free(session_test_threads);
  session_test_threads = NULL;

  log_info("Completed %d/%d session tests", session_test_count, session_test_count);

  return 0;
}

static int session_start_case(void *(*func)(void *), const char *name) {
  assert(session_test_threads != NULL);
  log_trace("Starting test %d, named: %s", session_test_count, name);

  assert(pthread_create(&session_test_threads[session_test_count++], NULL, func, NULL) == 0);

  return 0;
}
