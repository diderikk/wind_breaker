#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <stdio.h>
#include <time.h>

typedef enum { TRACE, DEBUG, INFO, WARN, ERROR } LOG_LEVEL;
typedef enum { CONSOLE_ONLY, FILE_ONLY, CONSOLE_FILE } LOG_DESTINATION;

FILE *logger_init(LOG_LEVEL level, LOG_DESTINATION destination,
                  char *file_path);
void log_logo();
void log_trace_(const char *file, const char *format, ...);
void log_debug_(const char *file, const char *format, ...);
void log_info_(const char *file, const char *format, ...);
void log_warn_(const char *file, const char *format, ...);
void log_error_(const char *file, const char *format, ...);

// Macro definitions
#define log_trace(format, ...) log_trace_(__FILE__, format, ##__VA_ARGS__)
#define log_debug(format, ...) log_debug_(__FILE__, format, ##__VA_ARGS__)
#define log_info(format, ...) log_info_(__FILE__, format, ##__VA_ARGS__)
#define log_warn(format, ...) log_warn_(__FILE__, format, ##__VA_ARGS__)
#define log_error(format, ...) log_error_(__FILE__, format, ##__VA_ARGS__)

#endif // LOGGER_H
