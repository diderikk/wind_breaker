#include "assert2.h"
#include "logger.h"
#include <signal.h>
#include <stdarg.h>

void assert_(const char *file, int line, const char *func, const char *msg) {
  log_error("Assertion failed: %s:%d: %s: %s\n", file, line, func, msg);
  raise(SIGABRT);
}

void assert_log_(const char *file, int line, const char *func, const char *msg,
                 const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_error(format, args);
  va_end(args);

  assert_(file, line, func, msg);
}
