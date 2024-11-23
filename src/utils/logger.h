#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <pthread.h>

typedef enum { TRACE, DEBUG, INFO, WARN, ERROR } LOG_LEVEL;

void log_trace(const char *format, ...);
void log_debug(const char *format, ...);
void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_error(const char *format, ...);


#endif // LOGGER_H
