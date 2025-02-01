#ifndef ASSERT2_H
#define ASSERT2_H

void assert_(const char *file, int line, const char *func, const char *msg);
void assert_log_(const char *file, int line, const char *func, const char *msg,
                 const char *format, ...);

#define assert_log(expr, format, ...)                                          \
  ((void)((expr) || (assert_log_(__FILE__, __LINE__, __func__, #expr, format,  \
                                 ##__VA_ARGS__),                               \
                     0)))

#define assert(expr)                                                           \
  ((void)((expr) || (assert_(__FILE__, __LINE__, __func__, #expr), 0)))

#endif // ASSERT2_H
