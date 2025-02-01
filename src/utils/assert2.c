#include "assert2.h"
#include "logger.h"
#include <stdarg.h>
#include <stdlib.h>

void assert_(const char *file, int line, const char *func, const char *msg) {
  log_error("Assertion failed: %s:%d: %s: %s\n", file, line, func, msg);
  exit(EXIT_FAILURE);
}

void assert_log_(const char *file, int line, const char *func, const char *msg,
                 const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_error(format, args);
  va_end(args);

  assert_(file, line, func, msg);
}
