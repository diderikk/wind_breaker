#include "../../../src/utils/assert2.h"
#include "../../../src/utils/logger.h"
#include "../../../src/http/request.h"
#include "../../../src/properties.h"
#include <string.h>

static int request_test_count = 0;
static int request_start_case(void *(*func)(void *), const char *name);

void * uri_test() {
  assert(strcmp(uri_to_file_name("/"), "index.html") == 0);
  assert(strcmp(uri_to_file_name("/favicon"), "favicon.png") == 0);

  return NULL;
}

int request_test() {
  request_start_case(uri_test, "uri_test");


  return request_test_count;
}

static int request_start_case(void *(*func)(void *), const char *name) {
  log_trace("Starting test %d, named: %s", request_test_count++, name);

  func(NULL);

  return 0;
}
