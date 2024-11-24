#include "logger.h"
#include <stdarg.h>

char *log_level_to_string(LOG_LEVEL level);
void log_message(LOG_LEVEL level, const char *message, va_list args_f,
                 va_list args_c);

static LOG_LEVEL log_level = TRACE;
static LOG_DESTINATION log_destination = CONSOLE_ONLY;
static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

FILE *logger_init(LOG_LEVEL level, LOG_DESTINATION destination,
                  char *file_path) {
  log_level = level;
  log_destination = destination;

  if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
    printf("Opening log file: %s\n", file_path);
    log_file = fopen(file_path, "a");
    if (log_file == NULL) {
      fprintf(stderr, "Failed to open log file: %s\n", file_path);
      perror("fopen");
      return NULL;
    }
  }
  printf("Logger fd: %d\n", fileno(log_file));

  log_info("Logger initialized with level %s and destination %s",
           log_level_to_string(level),
           log_destination == CONSOLE_ONLY ? "CONSOLE_ONLY"
           : log_destination == FILE_ONLY  ? "FILE_ONLY"
                                           : "CONSOLE_FILE");
  return log_file;
}

void log_trace(const char *message, ...) {
  if (log_level > TRACE) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(TRACE, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_debug(const char *message, ...) {
  if (log_level > DEBUG) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(DEBUG, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_info(const char *message, ...) {
  if (log_level > INFO) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(INFO, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_warn(const char *message, ...) {
  if (log_level > WARN) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(WARN, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_error(const char *message, ...) {
  va_list args;
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(ERROR, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_message(LOG_LEVEL level, const char *message, va_list args_f,
                 va_list args_c) {
  time_t rawtime;
  struct tm *timeinfo;
  char time_str[20];
  pthread_t thread_id = pthread_self();

  // Get the current time
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // Format the time as YYYY-MM-DD HH:MM:SS
  strftime(time_str, sizeof(time_str), "%Y%m%d %H:%M:%S", timeinfo);

  pthread_mutex_lock(&log_mutex);

  if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
    if (log_file != NULL) {
      fprintf(log_file, "%s (%s) [%lu]: ", time_str, log_level_to_string(level),
              (unsigned long)thread_id);
      vfprintf(log_file, message, args_f);
      fprintf(log_file, "\n");
      fflush(log_file);
    }
  }
  if (log_destination == CONSOLE_ONLY || log_destination == CONSOLE_FILE) {
    fprintf(stdout, "%s (%s) [%lu]: ", time_str, log_level_to_string(level),
            (unsigned long)thread_id);
    vfprintf(stdout, message, args_c);
    fprintf(stdout, "\n");
  }

  pthread_mutex_unlock(&log_mutex);
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
