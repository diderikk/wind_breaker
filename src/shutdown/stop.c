#include "stop.h"
#include "assert.h"
#include <stddef.h>
#include <string.h>

static volatile sig_atomic_t server_stop = 0;

sig_atomic_t stop() { return server_stop; }

void _stop_server(const char *file) {
  assert(file != NULL);

  if (strstr(file, "main.c") == NULL) {
    return;
  }

  server_stop = 1;
}
