#include "../../src/utils/logger.h"
#include "../../src/properties.h"
#include "data_structures/poll_array_test.c"
#include "data_structures/session_test.c"
#include "data_structures/worker_queue_test.c"
#include "http/request_test.c"


int main() {
  set_env("test");
  int number_of_tests = 0;

  number_of_tests += poll_array_test();
  number_of_tests += session_test();
  number_of_tests += worker_queue_test();
  number_of_tests += request_test();
  
  printf("\n");
  log_info("Number of tests passed: %d", number_of_tests);

  return 0;
}
