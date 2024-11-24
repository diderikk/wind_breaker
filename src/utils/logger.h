#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <stdio.h>
#include <time.h>

typedef enum { TRACE, DEBUG, INFO, WARN, ERROR } LOG_LEVEL;
typedef enum { CONSOLE_ONLY, FILE_ONLY, CONSOLE_FILE } LOG_DESTINATION;

FILE *logger_init(LOG_LEVEL level, LOG_DESTINATION destination,
                  char *file_path);
void log_trace(const char *format, ...);
void log_debug(const char *format, ...);
void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_error(const char *format, ...);

#endif // LOGGER_H
