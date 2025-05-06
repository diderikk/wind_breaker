#include "../../../src/data_structures/session.h"
#include "../../../src/data_structures/marked_fds.h"
#include "../../../src/utils/assert2.h"
#include "../../../src/static.h"
#include "../../../src/properties.h"
#include <pthread.h>
#include <stdlib.h>

#define TEST_MAX_SIZE 5


static int session_test_count = 0;
static int session_start_case(void *(*func)(void *), const char *name);

void *init_session_test() {
  init_session_cache(NULL);

  return NULL;
}

void *destroy_session_test() {
  destroy_session_cache();

  return NULL;
}

void* push_request_initial_test() {
    init_session_cache(NULL);
    push_request(1, WORK_STATUS_INITIAL);
    destroy_session_cache();

    return NULL;
}

void* pop_request_by_fd_initial_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    push_request(1, WORK_STATUS_INITIAL);
    struct session_full_return sess = pop_request_by_fd(1);
    assert(sess.session != NULL);
    assert(sess.session->related_fd == 1);
    assert(sess.session->thread_id == 0);
    assert(sess.session->id != 0);
    assert(sess.request->uri[0] == 0);
    assert(sess.response->body[0] == 0);
    assert(sess.in_buffer[0] == 0);
    assert(sess.out_buffer[0] == 0);
    assert(sess.bio != NULL);
    assert(sess.ssl == NULL);
    destroy_session_cache();

    return NULL;
}

void* pop_request_by_fd_initial_with_ssl_test() {
    set_session_max_size(TEST_MAX_SIZE);
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    assert(ctx != NULL);
    
    init_session_cache(ctx);
    push_request(1, WORK_STATUS_INITIAL);
    struct session_full_return sess = pop_request_by_fd(1);
    assert(sess.session != NULL);
    destroy_session_cache();

    return NULL;
}

void* push_request_read_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    push_request(1, WORK_STATUS_INITIAL);
    pop_request_by_fd(1);
    push_request(1, WORK_STATUS_REQUEST_READ);
    destroy_session_cache();

    return NULL;
}

void* pop_request_read_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    push_request(1, WORK_STATUS_INITIAL);
    pop_request_by_fd(1);
    push_request(1, WORK_STATUS_REQUEST_READ);
    pop_request(WORK_STATUS_REQUEST_READ);
    destroy_session_cache();

    return NULL;
}

void* pop_request_full_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    push_request(1, WORK_STATUS_INITIAL);
    pop_request_by_fd(1);
    push_request(1, WORK_STATUS_REQUEST_READ);
    pop_request(WORK_STATUS_REQUEST_READ);
    push_request(1, WORK_STATUS_PARSED);
    pop_request(WORK_STATUS_PARSED);
    push_request(1, WORK_STATUS_DATA_FETCHED);
    pop_request(WORK_STATUS_DATA_FETCHED);
    push_request(1, WORK_STATUS_READY_TO_SEND);
    pop_request(WORK_STATUS_READY_TO_SEND);
    push_request(1, WORK_STATUS_SENT);
    pop_request_by_fd(1);
    destroy_session_cache();

    return NULL;
}

void* push_request_rejected_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    push_request(1, WORK_STATUS_INITIAL);
    pop_request_by_fd(1);
    push_request(1, WORK_STATUS_REJECTED);
    struct session_full_return sess = pop_request_by_fd(1);
    assert(sess.session == NULL);
    destroy_session_cache();
    
    return NULL;
}

void* get_session_id_for_thread_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    push_request(1, WORK_STATUS_INITIAL);
    struct session_full_return sess = pop_request_by_fd(1);
    assert(sess.session != NULL);
    sess.session->thread_id = (unsigned long) pthread_self();
    int id = get_session_id_for_thread();
    assert(id == sess.session->id);
    destroy_session_cache();
    
    return NULL;
}

void* push_request_overflow_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    int sessions = TEST_MAX_SIZE + 3;
    //pthread_t threads[session_test_count];

    for (int fd = 1; fd < sessions + 1; fd++) {
        push_request(fd, WORK_STATUS_INITIAL);
    }

    int count = 0;
    for(int fd = sessions - TEST_MAX_SIZE + 1; fd < sessions + 1; fd++) {
        struct session_full_return sess = pop_request_by_fd(fd);
        assert(sess.session != NULL);
        assert(sess.session->related_fd == fd);
        assert(sess.session->thread_id == 0);
        assert(sess.session->id != 0);
        assert(sess.request->uri[0] == 0);
        assert(sess.response->body[0] == 0);
        assert(sess.in_buffer[0] == 0);
        assert(sess.out_buffer[0] == 0);
        assert(sess.bio != NULL);
        assert(sess.ssl == NULL);
        count++;
    } 

    assert(count == TEST_MAX_SIZE);

    destroy_session_cache();

    return NULL;
}

void* add_and_remove_all_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);

    for (int fd = 1; fd < TEST_MAX_SIZE + 1; fd++) {
        push_request(fd, WORK_STATUS_INITIAL);
    }


    for (int fd = 1; fd < TEST_MAX_SIZE + 1; fd++) {
      pop_request_by_fd(fd);
      push_request(fd, WORK_STATUS_INITIAL);   
    }

    assert(pop_request_by_fd(1).session == NULL);
    assert(pop_request_by_fd(1).bio == NULL);

    destroy_session_cache();

    return NULL;
}

void *get_session_for_thread_func(void* i) {
  int *fd = (int *)i;
  struct session_full_return sess = pop_request_by_fd(*fd);
  assert(sess.session != NULL);
  assert(sess.session->related_fd == *fd);
  assert(sess.session->id != 0);

  return NULL;
}

void* add_and_get_all_test() {
    set_session_max_size(TEST_MAX_SIZE);
    init_session_cache(NULL);
    pthread_t threads[TEST_MAX_SIZE];
    int fd[TEST_MAX_SIZE];

    for (int i = 1; i < TEST_MAX_SIZE + 1; i++) {
        push_request(i, WORK_STATUS_INITIAL);
    }

    for (int i = 1; i < TEST_MAX_SIZE + 1; i++) {
      fd[i-1] = i;
      pthread_create(&threads[i - 1], NULL, get_session_for_thread_func, &fd[i-1]);
    }

    for (int i = 0; i < TEST_MAX_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    destroy_session_cache();

    return NULL;
}


int session_test() {
  init_marked_fds();

  session_start_case(init_session_test, "init_session_test");
  session_start_case(destroy_session_test, "destroy_session_test");
  session_start_case(push_request_initial_test, "push_request_initial_test");
  session_start_case(pop_request_by_fd_initial_test, "pop_request_by_fd_initial_test");
  session_start_case(pop_request_by_fd_initial_with_ssl_test, "pop_request_by_fd_initial_with_ssl_test");
  session_start_case(push_request_read_test, "push_request_read_test");
  session_start_case(pop_request_read_test, "pop_request_read_test");
  session_start_case(pop_request_full_test, "pop_request_full_test");
  /*session_start_case(push_request_rejected_test, "push_request_rejected_test");*/
  /*session_start_case(get_session_id_for_thread_test, "get_session_id_for_thread_test");*/
  /*session_start_case(push_request_overflow_test, "push_request_overflow_test");*/
  /*session_start_case(add_and_remove_all_test, "add_and_remove_all_test");*/
  /*session_start_case(add_and_get_all_test, "add_and_get_all_test");*/

  destroy_marked_fds();

  return session_test_count;
}

static int session_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", session_test_count++, name);

  func(NULL);

  return 0;
}
