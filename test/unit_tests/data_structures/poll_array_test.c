#include "../../../src/utils/assert2.h"
#include "../../../src/data_structures/poll_array.h"
#include "../../../src/data_structures/marked_fds.h"
#include "../../../src/properties.h"
#include <pthread.h>
#include <stdlib.h>

static int poll_array_test_count = 0;

extern int count, last_in_index;
extern struct pollfd *fds;

static int poll_array_start_case(void *(*func)(void *), const char *name);

void * init_poll_array_test() {
  init_poll_array(1);
  assert(count == 1);
  assert(last_in_index == 1);
  assert(fds != NULL);
  assert(fds[0].fd == 1);
  assert(fds[0].events == POLLIN);
  destroy_poll_array();

  return NULL;
}

void * destroy_poll_array_test() {
  init_poll_array(1);
  destroy_poll_array();
  assert(fds == NULL);

  return NULL;
}

void *add_poll_fd_sync_test() {
    init_poll_array(1);

    add_poll_fd_sync(2);
    assert(fds[1].fd == 2);
    assert(fds[1].events == POLLIN);
    assert(count == 2);
    assert(last_in_index == 1);

    add_poll_fd_sync(3);
    assert(fds[2].fd == 3);
    assert(fds[2].events == POLLIN);
    assert(count == 3);
    assert(last_in_index == 1);

    destroy_poll_array();

    return NULL;
}

void *remove_poll_fd_by_index_sync_test() {
    init_poll_array(1);

    add_poll_fd_sync(2);
    add_poll_fd_sync(3);

    int index = 1;
    remove_poll_fd_by_index_sync(&index);
    assert(count == 2);
    assert(last_in_index == 1);
    assert(fds[0].fd == 1);
    assert(fds[1].fd == 3); // fd 2 should be removed
    assert(index == 0);
    assert(fds[2].fd == 0);

    destroy_poll_array();

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
void *add_poll_fd_sync_overflow_test() {
    set_session_max_size(10);
    
    init_poll_array(1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(i);
    }

    assert(count == 10); // max_count is 10
    assert(fds[1].fd == 11);
    assert(fds[2].fd == 12);
    assert(fds[3].fd == 4);
    assert(last_in_index == 3);

    destroy_poll_array();

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor after last_in_index
void *add_poll_fd_sync_overflow_minus_one_after_last_test() {
    set_session_max_size(10);
    
    init_poll_array(1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(i);
    }

    int index = 6;
    remove_poll_fd_by_index_sync(&index);

    assert(count == 9);
    assert(last_in_index == 3);

    destroy_poll_array();

    return NULL;
}


// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor before last_in_index
void *add_poll_fd_sync_overflow_minus_one_before_last_test() {
    set_session_max_size(10);
    
    init_poll_array(1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(i);
    }

    int index = 1;
    remove_poll_fd_by_index_sync(&index);

    assert(count == 9);
    assert(last_in_index == 2);

    destroy_poll_array();

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor before last_in_index
void *add_poll_fd_sync_overflow_minus_all() {
    set_session_max_size(10);
    
    init_poll_array(1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(i);
    }

    for (int i = count - 1; i > 0; i--) {
      remove_poll_fd_by_index_sync(&i);
      i++;
    }

    assert(count == 1);
    assert(last_in_index == 1);

    for(int i = 1; i < 10; i++) {
      assert(fds[i].fd == 0);
    }

    destroy_poll_array();

    return NULL;
}

// Edge case: Test adding more file descriptors than max_count
// and removing a file descriptor before last_in_index
void *add_poll_fd_sync_overflow_minus_even() {
    set_session_max_size(10);
    
    init_poll_array(1);

    for (int i = 2; i <= 12; i++) {
        add_poll_fd_sync(i);
    }

    for (int i = 8; i > 0; i -= 2) {
      remove_poll_fd_by_index_sync(&i);
      i++;
    }

    assert(count == 6);
    assert(last_in_index == 2);

    for(int i = 1; i < 10; i++) {
      if(i < 6)
        assert(fds[i].fd != 0);
      else
        assert(fds[i].fd == 0);
    }

    destroy_poll_array();

    return NULL;
}

int poll_array_test() {
  init_marked_fds();

  poll_array_start_case(init_poll_array_test, "init_poll_array_test");
  poll_array_start_case(destroy_poll_array_test, "destroy_poll_array_test");
  poll_array_start_case(add_poll_fd_sync_test, "add_poll_fd_sync_test");
  poll_array_start_case(remove_poll_fd_by_index_sync_test, "remove_poll_fd_by_index_sync_test");
  poll_array_start_case(add_poll_fd_sync_overflow_test, "add_poll_fd_sync_overflow_test");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_one_after_last_test, "add_poll_fd_sync_overflow_minus_one_after_last_test");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_one_before_last_test, "add_poll_fd_sync_overflow_minus_one_before_last_test");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_all, "add_poll_fd_sync_overflow_minus_all");
  poll_array_start_case(add_poll_fd_sync_overflow_minus_even, "add_poll_fd_sync_overflow_minus_even");

  destroy_marked_fds();
  return poll_array_test_count;
}


static int poll_array_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", poll_array_test_count++, name);

  func(NULL);

  return 0;
}
