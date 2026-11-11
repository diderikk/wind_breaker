#include "marked_fds.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Marked file descriptors are closed sessions
 * They are marked so they can be removed from
 * poll_array structure
 */
static int *marks = NULL;

void init_marked_fds() {
  marks = calloc(MAX_FD, sizeof(int));
  if (marks == NULL) {
    perror("Failed to allocate memory for marks");
    exit(EXIT_FAILURE);
  }
}
void destroy_marked_fds() {
  free(marks);
  marks = NULL;
}

void mark(int fd) { __sync_fetch_and_add(&marks[fd], 1); }

void unmark(int fd) { __sync_fetch_and_and(&marks[fd], 0); }

char is_marked(int fd) { return __sync_fetch_and_add(&marks[fd], 0) > 0; }
