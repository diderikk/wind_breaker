#include "marked_fds.h"
#include "../properties.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static int *marks = NULL;
static int max_count = 0;

void init_marked_fds() {
  max_count = get_session_max_size() * 2;
  marks = calloc(max_count, sizeof(int));
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
