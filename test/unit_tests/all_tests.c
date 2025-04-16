#include "../../src/properties.h"
#include "../../src/utils/logger.h"
#include "data_structures/poll_array_test.c"
#include "data_structures/session_test.c"
#include "data_structures/worker_queue_test.c"
#include "http/request_test.c"
#include "http/response_test.c"
#include "utils/compression_test.c"
#include "utils/hash_test.c"


int main() {
  set_env("test");
  init_logger(TRACE, CONSOLE_ONLY, NULL);
  int number_of_tests = 0;

  number_of_tests += poll_array_test();
  number_of_tests += session_test();
  number_of_tests += worker_queue_test();
  number_of_tests += compression_test();
  number_of_tests += hash_test();
  number_of_tests += request_test();
  number_of_tests += response_test();
  
  printf("\nNumber of tests passed: %d\n", number_of_tests);

  return 0;
}
