#include "../../src/utils/logger.h"
#include "poll_array_test.c"
#include "session_test.c"


int main() {
  int number_of_tests = 0;

  number_of_tests += poll_array_test();
  number_of_tests += session_test();

  printf("\n");
  log_info("Number of tests passed: %d", number_of_tests);

  return 0;
}
