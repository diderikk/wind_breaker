#include "stop.h"
#include "assert.h"
#include <stddef.h>
#include <string.h>

static volatile sig_atomic_t shutdown_requested = 0;

sig_atomic_t is_shutdown_requested() { return shutdown_requested; }

void _request_shutdown(const char *file) {
  assert(file != NULL);

  if (strstr(file, "main.c") == NULL) {
    return;
  }

  shutdown_requested = 1;
}
