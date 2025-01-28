#include "../../src/utils/assert2.h"
#include "../../src/utils/logger.h"
#include "../../src/data_structures/poll_array.h"
#include "../../src/properties.h"
#include <pthread.h>
#include <stdlib.h>

static int poll_array_test_count = 0;
static pthread_t *poll_array_test_threads = NULL;

static int poll_array_start_case(void *(*func)(void *), const char *name);

void * init_poll_array_test() {
  poll_array *pa = NULL;
  init_poll_array(&pa, 1);
  assert(pa != NULL);
  assert(pa->fds != NULL);
  assert(pa->fds[0].fd == 1);
  assert(pa->fds[0].events == POLLIN);
  assert(pa->count == 1);
  assert(pa->last_in_index == 1);
  destroy_poll_array(&pa);

  return NULL;
}

void * destroy_poll_array_test() {
  poll_array *pa = NULL;
  init_poll_array(&pa, 1);
  destroy_poll_array(&pa);
  assert(pa == NULL);

  return NULL;
}

void *add_poll_fd_sync_test() {
    poll_array *pa = NULL;
    init_poll_array(&pa, 1);

    add_poll_fd_sync(pa, 2);
    assert(pa->fds[1].fd == 2);
    assert(pa->fds[1].events == POLLIN);
    assert(pa->count == 2);
    assert(pa->last_in_index == 1);

    add_poll_fd_sync(pa, 3);
    assert(pa->fds[2].fd == 3);
    assert(pa->fds[2].events == POLLIN);
    assert(pa->count == 3);
    assert(pa->last_in_index == 1);

    destroy_poll_array(&pa);

    return NULL;
}

void *remove_poll_fd_by_index_sync_test() {
    poll_array *pa = NULL;
    init_poll_array(&pa, 1);

    add_poll_fd_sync(pa, 2);
    add_poll_fd_sync(pa, 3);

    int index = 1;
    remove_poll_fd_by_index_sync(pa, &index);
    assert(pa->count == 2);
    assert(pa->last_in_index == 1);
    assert(pa->fds[0].fd == 1);
    assert(pa->fds[1].fd == 3); // fd 2 should be removed
    assert(index == 0);
    assert(pa->fds[2].fd == 0);

    destroy_poll_array(&pa);

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
void *add_poll_fd_sync_overflow_test() {
    set_poll_array_max_size(10);
    poll_array *pa = NULL;
    
    init_poll_array(&pa, 1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(pa, i);
    }

    assert(pa->count == 10); // max_count is 10
    assert(pa->fds[1].fd == 11);
    assert(pa->fds[2].fd == 12);
    assert(pa->fds[3].fd == 4);
    assert(pa->last_in_index == 3);

    destroy_poll_array(&pa);

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor after last_in_index
void *add_poll_fd_sync_overflow_minus_one_after_last_test() {
    set_poll_array_max_size(10);
    poll_array *pa = NULL;
    
    init_poll_array(&pa, 1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(pa, i);
    }

    int index = 6;
    remove_poll_fd_by_index_sync(pa, &index);

    assert(pa->count == 9);
    assert(pa->last_in_index == 3);

    destroy_poll_array(&pa);

    return NULL;
}


// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor before last_in_index
void *add_poll_fd_sync_overflow_minus_one_before_last_test() {
    set_poll_array_max_size(10);
    poll_array *pa = NULL;
    
    init_poll_array(&pa, 1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(pa, i);
    }

    int index = 1;
    remove_poll_fd_by_index_sync(pa, &index);

    assert(pa->count == 9);
    assert(pa->last_in_index == 2);

    destroy_poll_array(&pa);

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor before last_in_index
void *add_poll_fd_sync_overflow_minus_all() {
    set_poll_array_max_size(10);
    poll_array *pa = NULL;
    
    init_poll_array(&pa, 1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(pa, i);
    }

    for (int i = pa->count - 1; i > 0; i--) {
      remove_poll_fd_by_index_sync(pa, &i);
      i++;
    }

    assert(pa->count == 1);
    assert(pa->last_in_index == 1);

    for(int i = 1; i < 10; i++) {
      assert(pa->fds[i].fd == 0);
    }

    destroy_poll_array(&pa);

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor before last_in_index
void *add_poll_fd_sync_overflow_minus_even() {
    set_poll_array_max_size(10);
    poll_array *pa = NULL;
    
    init_poll_array(&pa, 1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(pa, i);
    }

    for (int i = 8; i > 0; i -= 2) {
      remove_poll_fd_by_index_sync(pa, &i);
      i++;
    }

    assert(pa->count == 6);
    assert(pa->last_in_index == 2);

    for(int i = 1; i < 10; i++) {
      if(i < 6)
        assert(pa->fds[i].fd != 0);
      else
        assert(pa->fds[i].fd == 0);
    }

    destroy_poll_array(&pa);

    return NULL;
}

int poll_array_test() {

  poll_array_test_threads = (pthread_t *)malloc(sizeof(pthread_t) * 100);
  poll_array_start_case(init_poll_array_test, "init_poll_array_test");
  poll_array_start_case(destroy_poll_array_test, "destroy_poll_array_test");
  poll_array_start_case(add_poll_fd_sync_test, "add_poll_fd_sync_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_test, "remove_poll_fd_by_index_sync_test");
  poll_array_start_case(add_poll_fd_sync_overflow_test, "add_poll_fd_sync_overflow_test");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_one_after_last_test, "add_poll_fd_sync_overflow_minus_one_after_last_test");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_one_before_last_test, "add_poll_fd_sync_overflow_minus_one_before_last_test");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_all, "add_poll_fd_sync_overflow_minus_all");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_even, "add_poll_fd_sync_overflow_minus_even");

  for (int i = 0; i < poll_array_test_count; i++) {
    pthread_join(poll_array_test_threads[i], NULL);
  }

  log_info("Completed %d/%d poll_array tests", poll_array_test_count, poll_array_test_count);
  free(poll_array_test_threads);
  poll_array_test_threads = NULL;

  return poll_array_test_count;
}


static int poll_array_start_case(void *(*func)(void *), const char *name) {
  assert(poll_array_test_threads != NULL);
  log_trace("Starting test %d, named: %s", poll_array_test_count, name);

  assert(pthread_create(&poll_array_test_threads[poll_array_test_count++], NULL, func, NULL) == 0);

  return 0;
}
