#include "../../../src/data_structures/worker_queue.h"
#include "../../../src/utils/assert2.h"
#include "../../../src/properties.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_MAX_SIZE 5
#define INT_SECOND_MOST_SIGNIFICANT_BIT 1 << 30

static int worker_queue_test_count = 0;
static int worker_queue_start_case(void *(*func)(void *), const char *name);

void *init_destroy_worker_queue_test() {
  init_queue();

  destroy_queue();
  return NULL;
}

void *push_queue_test() {
  init_queue();

  char data[REQUEST_RESPONSE_MAX_SIZE];
  memset(data, 'a', REQUEST_RESPONSE_MAX_SIZE);
  queue_push(1, data, REQUEST_RESPONSE_MAX_SIZE);

  destroy_queue();
  return NULL;
}

void *pop_queue_test() {
  init_queue();


  char data[REQUEST_RESPONSE_MAX_SIZE];
  memset(data, 'a', REQUEST_RESPONSE_MAX_SIZE);
  queue_push(1, data, REQUEST_RESPONSE_MAX_SIZE);
  worker_data *wd = queue_pop();
  assert(wd != NULL);
  assert(wd->fd == 1);
  assert(strncmp(wd->data, "aaaa", 4) == 0);

  destroy_queue();
  return NULL;
}

void * push_queue_pop_test() {
  init_queue();

  for (int i = 0; i < QUEUE_MAX_SIZE + 3; i++) {
    char data[REQUEST_RESPONSE_MAX_SIZE];
    memset(data, 'a', REQUEST_RESPONSE_MAX_SIZE);
    queue_push(i, data, REQUEST_RESPONSE_MAX_SIZE);
    worker_data *wd = queue_pop();
    assert(wd != NULL);
    assert(wd->fd == i);
    assert(strncmp(wd->data, "aaaa", 4) == 0);
  }


  destroy_queue();
  return NULL;
}

void * push_full_then_pop_all_test() {
  init_queue();

  for (int i = 0; i < QUEUE_MAX_SIZE; i++) {
    char data[REQUEST_RESPONSE_MAX_SIZE];
    memset(data, 'a', REQUEST_RESPONSE_MAX_SIZE);
    queue_push(i, data, REQUEST_RESPONSE_MAX_SIZE);
  }

  for (int i = 0; i < QUEUE_MAX_SIZE; i++) {
    worker_data *wd = queue_pop();
    assert(wd != NULL);
    assert(wd->fd == i);
    assert(strncmp(wd->data, "aaaa", 4) == 0);
  }

  destroy_queue();
  return NULL;
}

void * push_some_then_pop_some_test() {
  init_queue();

  for (int i = 0; i < QUEUE_MAX_SIZE; i++) {
    char data[REQUEST_RESPONSE_MAX_SIZE];
    memset(data, 'a', REQUEST_RESPONSE_MAX_SIZE);
    queue_push(i, data, REQUEST_RESPONSE_MAX_SIZE);
  }

  for (int i = 0; i < QUEUE_MAX_SIZE - 2; i++) {
    worker_data *wd = queue_pop();
    assert(wd != NULL);
    assert(wd->fd == i);
    assert(strncmp(wd->data, "aaaa", 4) == 0);
  }

  for (int i = 0; i < 1; i++) {
    char data[REQUEST_RESPONSE_MAX_SIZE];
    memset(data, 'a', REQUEST_RESPONSE_MAX_SIZE);
    queue_push(i, data, REQUEST_RESPONSE_MAX_SIZE);
  }

  worker_data *wd = queue_pop();
  assert(wd != NULL);
  assert(wd->fd == QUEUE_MAX_SIZE - 2);
  assert(strncmp(wd->data, "aaaa", 4) == 0);

  destroy_queue();
  return NULL;
}

void * set_work_ready_test() {
  init_queue();

  char data[REQUEST_RESPONSE_MAX_SIZE];
  memset(data, 'a', REQUEST_RESPONSE_MAX_SIZE);
  queue_push(1 | INT_SECOND_MOST_SIGNIFICANT_BIT, data, REQUEST_RESPONSE_MAX_SIZE);
  set_work_ready(1 | INT_SECOND_MOST_SIGNIFICANT_BIT);
  worker_data *wd = queue_pop();
  assert(wd != NULL);
  assert(wd->fd == 1);


  destroy_queue();
  return NULL;
}

int worker_queue_test() {
  set_queue_max_size(QUEUE_MAX_SIZE); 
  worker_queue_start_case(init_destroy_worker_queue_test, "init_destroy_worker_queue_test");
  worker_queue_start_case(push_queue_test, "push_queue_test");
  worker_queue_start_case(pop_queue_test, "pop_queue_test");
  worker_queue_start_case(set_work_ready_test, "set_work_ready_test");
  worker_queue_start_case(push_queue_pop_test, "push_queue_pop_test");
  worker_queue_start_case(push_full_then_pop_all_test, "push_full_then_pop_all_test");
  worker_queue_start_case(push_some_then_pop_some_test, "push_some_then_pop_some_test");

  return worker_queue_test_count;
}

static int worker_queue_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", worker_queue_test_count++, name);

  func(NULL);

  return 0;
}
