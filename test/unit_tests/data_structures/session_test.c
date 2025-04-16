#include "../../../src/data_structures/session.h"
#include "../../../src/utils/assert2.h"
#include "../../../src/static.h"
#include <pthread.h>
#include <stdlib.h>

#define TEST_MAX_SIZE 5


static int session_test_count = 0;
static int session_start_case(void *(*func)(void *), const char *name);

void *init_session_test() {
  init_session_cache(3, NULL);

  return NULL;
}

void *destroy_session_test() {
  destroy_session_cache();

  return NULL;
}

void* add_to_session_sync_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);
    add_to_session_sync(1);
    destroy_session_cache();

    return NULL;
}

void* get_session_for_thread_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);
    add_to_session_sync(1);
    add_thread_to_session(1);
    struct session_full_return sess = get_session_for_thread();
    assert(sess.session != NULL);
    assert(sess.session->related_fd == 1);
    assert(sess.session->thread_id == (unsigned long) pthread_self());
    assert(sess.session->id != 0);
    assert(sess.bio != NULL);
    assert(sess.ssl == NULL);
    destroy_session_cache();

    return NULL;
}

void* get_session_for_thread_with_ssl_test() {
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    assert(ctx != NULL);
    
    init_session_cache(TEST_MAX_SIZE, ctx);
    add_to_session_sync(1);
    add_thread_to_session(1);
    struct session_full_return sess = get_session_for_thread();
    assert(sess.session != NULL);
    assert(sess.session->related_fd == 1);
    assert(sess.session->thread_id == (unsigned long) pthread_self());
    assert(sess.session->id != 0);
    assert(sess.bio != NULL);
    assert(sess.ssl != NULL);
    destroy_session_cache();

    return NULL;
}

void* remove_thread_from_session_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);
    add_to_session_sync(1);
    add_thread_to_session(1);
    remove_thread_from_session();
    struct session_full_return sess = get_session_for_thread();
    assert(sess.session == NULL);
    assert(sess.bio == NULL);
    destroy_session_cache();

    return NULL;
}

void* add_thread_to_session_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);
    add_to_session_sync(1);
    add_thread_to_session(1);

    destroy_session_cache();

    return NULL;
}

void* del_from_session_sync_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);
    add_to_session_sync(1);
    add_thread_to_session(1);
    del_from_session_sync(1);

    assert(get_session_for_thread().session == NULL);
    assert(get_session_for_thread().bio == NULL);

    destroy_session_cache();
    
    return NULL;
}

void* add_to_session_sync_overflow_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);
    int sessions = TEST_MAX_SIZE + 3;
    //pthread_t threads[session_test_count];

    for (int fd = 1; fd < sessions + 1; fd++) {
        add_to_session_sync(fd);
    }

    int count = 0;
    for(int fd = sessions - TEST_MAX_SIZE + 1; fd < sessions + 1; fd++) {
        add_thread_to_session(fd);
        struct session_full_return sess = get_session_for_thread();
        assert(sess.session != NULL);
        assert(sess.session->related_fd == fd);
        assert(sess.session->thread_id == (unsigned long) pthread_self());
        assert(sess.session->id != 0);
        assert(sess.bio != NULL);
        assert(sess.ssl == NULL);
        remove_thread_from_session();
        count++;
    } 

    assert(count == TEST_MAX_SIZE);

    destroy_session_cache();

    return NULL;
}

void* add_and_remove_all_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);

    for (int fd = 1; fd < TEST_MAX_SIZE + 1; fd++) {
        add_to_session_sync(fd);
    }

    add_thread_to_session(1);

    for (int fd = 1; fd < TEST_MAX_SIZE + 1; fd++) {
        del_from_session_sync(fd);
    }

    assert(get_session_for_thread().session == NULL);
    assert(get_session_for_thread().bio == NULL);

    destroy_session_cache();

    return NULL;
}

void *get_session_for_thread_func(void* i) {
  int *fd = (int *)i;
  add_thread_to_session(*fd);
    struct session_full_return sess = get_session_for_thread();
    assert(sess.session != NULL);
    assert(sess.session->related_fd == *fd);
    assert(sess.session->thread_id == (unsigned long) pthread_self());
    assert(sess.session->id != 0);

    return NULL;
}

void* add_and_get_all_test() {
    init_session_cache(TEST_MAX_SIZE, NULL);
    pthread_t threads[TEST_MAX_SIZE];
    int fd[TEST_MAX_SIZE];

    for (int i = 1; i < TEST_MAX_SIZE + 1; i++) {
        add_to_session_sync(i);
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
  session_start_case(init_session_test, "init_session_test");
  session_start_case(destroy_session_test, "destroy_session_test");
  session_start_case(add_to_session_sync_test, "add_to_session_sync_test");
  session_start_case(get_session_for_thread_test, "get_session_for_thread_test");
  session_start_case(get_session_for_thread_with_ssl_test, "get_session_for_thread_with_ssl_test");
  session_start_case(remove_thread_from_session_test, "remove_thread_from_session_test");
  session_start_case(add_thread_to_session_test, "add_thread_to_session_test");
  session_start_case(del_from_session_sync_test, "del_from_session_sync_test");
  session_start_case(add_to_session_sync_overflow_test, "add_to_session_sync_overflow_test");
  session_start_case(add_and_remove_all_test, "add_and_remove_all_test");
  session_start_case(add_and_get_all_test, "add_and_get_all_test");

  printf("Completed %d/%d session tests\n", session_test_count, session_test_count);

  return session_test_count;
}

static int session_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test %d, named: %s\n", session_test_count++, name);

  func(NULL);

  return 0;
}
