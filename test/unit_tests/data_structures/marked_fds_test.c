#include "../../../src/data_structures/marked_fds.h"
#include "../../../src/utils/assert2.h"
#include <stdio.h>

static int marked_fds_test_count = 0;
static int marked_fds_start_case(void *(*func)(void *), const char *name);

void *init_marked_fds_test() {
  init_marked_fds();

  destroy_marked_fds();

  return NULL;
}

void *mark_fd_test() {
  init_marked_fds();

  mark(1);

  assert(is_marked(1) == 1);

  destroy_marked_fds();

  return NULL;
}

void *mark_fds_test() {
  init_marked_fds();

  mark(1);
  mark(2);
  mark(3);

  assert(is_marked(1) == 1);
  assert(is_marked(2) == 1);
  assert(is_marked(3) == 1);

  destroy_marked_fds();

  return NULL;
}

void *unmark_fd_test() {
  init_marked_fds();

  mark(1);

  assert(is_marked(1) == 1);

  unmark(1);

  assert(is_marked(1) == 0);

  destroy_marked_fds();

  return NULL;
}

void *unmark_fds_test() {
  init_marked_fds();

  mark(1);
  mark(2);
  mark(3);

  assert(is_marked(1) == 1);
  assert(is_marked(2) == 1);
  assert(is_marked(3) == 1);

  unmark(1);
  unmark(2);
  unmark(3);

  assert(is_marked(1) == 0);
  assert(is_marked(2) == 0);
  assert(is_marked(3) == 0);

  destroy_marked_fds();

  return NULL;
}

int marked_fds_test() {

  marked_fds_start_case(init_marked_fds_test, "init_marked_fds_test");
  marked_fds_start_case(mark_fd_test, "mark_fd_test");
  marked_fds_start_case(mark_fds_test, "mark_fds_test");
  marked_fds_start_case(unmark_fd_test, "unmark_fd_test");
  marked_fds_start_case(unmark_fds_test, "unmark_fds_test");

  return marked_fds_test_count;
}


static int marked_fds_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", marked_fds_test_count++, name);

  func(NULL);

  return 0;
}
