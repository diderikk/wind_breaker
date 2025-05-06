#include "../../src/properties.h"
#include "../../src/utils/logger.h"
#include "data_structures/poll_array_test.c"
#include "data_structures/session_test.c"
#include "http/request_test.c"
#include "http/response_test.c"
#include "utils/compression_test.c"
#include "utils/hash_test.c"

#define GRN "\x1B[32m"
#define RESET "\x1B[0m"

int main() {
  set_env("test");
  init_logger(TRACE, CONSOLE_ONLY, NULL);
  int prev_number_of_tests = 0;
  int number_of_tests = 0;
  int group_count = 0;

  number_of_tests += poll_array_test();
  group_count = number_of_tests - prev_number_of_tests;
  printf(GRN "\nCompleted %d/%d poll array tests\n" RESET, group_count, group_count);
  prev_number_of_tests = number_of_tests;

  number_of_tests += session_test();
  group_count = number_of_tests - prev_number_of_tests;
  printf(GRN "\nCompleted %d/%d session tests\n" RESET, group_count, group_count);
  prev_number_of_tests = number_of_tests;

  number_of_tests += compression_test();
  group_count = number_of_tests - prev_number_of_tests;
  printf(GRN "\nCompleted %d/%d compression tests\n" RESET, group_count, group_count);
  prev_number_of_tests = number_of_tests;

  number_of_tests += hash_test();
  group_count = number_of_tests - prev_number_of_tests;
  printf(GRN "\nCompleted %d/%d hash tests\n" RESET, group_count, group_count);
  prev_number_of_tests = number_of_tests;

  number_of_tests += request_test();
  group_count = number_of_tests - prev_number_of_tests;
  printf(GRN "\nCompleted %d/%d request tests\n" RESET, group_count, group_count);
  prev_number_of_tests = number_of_tests;

  number_of_tests += response_test();
  group_count = number_of_tests - prev_number_of_tests;
  printf(GRN "\nCompleted %d/%d response tests\n" RESET, group_count, group_count);
  prev_number_of_tests = number_of_tests;
  
  printf(GRN "\nNumber of tests passed: %d\n" RESET, number_of_tests);

  return 0;
}
