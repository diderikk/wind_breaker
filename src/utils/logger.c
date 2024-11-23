#include "logger.h"
#include <stdarg.h>

char *log_level_to_string(LOG_LEVEL level);
void log_message(LOG_LEVEL level, const char *message, va_list args); 

void log_trace(const char *message, ...) {
  va_list args;
  va_start(args, message);
  log_message(TRACE, message, args);
  va_end(args);
}

void log_debug(const char *message, ...) {
  va_list args;
  va_start(args, message);
  log_message(DEBUG, message, args);
  va_end(args);
}

void log_info(const char *message, ...) {
  va_list args;
  va_start(args, message);
  log_message(INFO, message, args);
  va_end(args);
}

void log_warn(const char *message, ...) {
  va_list args;
  va_start(args, message);
  log_message(WARN, message, args);
  va_end(args);
}

void log_error(const char *message, ...) {
  va_list args;
  va_start(args, message);
  log_message(ERROR, message, args);
  va_end(args);
}

void log_message(LOG_LEVEL level, const char *message, va_list args) {
  time_t rawtime;
  struct tm *timeinfo;
  char time_str[20];
  pthread_t thread_id = pthread_self();

  // Get the current time
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // Format the time as YYYY-MM-DD HH:MM:SS
  strftime(time_str, sizeof(time_str), "%Y%m%d %H:%M:%S", timeinfo);
  
  
  fprintf(stdout, "%s (%s) [%lu]: ", time_str, log_level_to_string(level), (unsigned long) pthread_self());
  vfprintf(stdout, message, args);
  fprintf(stdout, "\n");
}

char *log_level_to_string(LOG_LEVEL level) {
  switch (level) {
    case TRACE:
      return "TRACE";
    case DEBUG:
      return "DEBUG";
    case INFO:
      return "INFO";
    case WARN:
      return "WARN";
    case ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}
