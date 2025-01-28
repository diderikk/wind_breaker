#include "logger.h"
#include "../data_structures/session.h"
#include <errno.h>
#include <stdarg.h>
#include <string.h>

char *log_level_to_string(LOG_LEVEL level);
void log_message(LOG_LEVEL level, const char *file, const char *message,
                 va_list args_f, va_list args_c);

static LOG_LEVEL log_level = TRACE;
static LOG_DESTINATION log_destination = CONSOLE_ONLY;
static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

FILE *logger_init(LOG_LEVEL level, LOG_DESTINATION destination,
                  const char *file_path) {
  log_level = level;
  log_destination = destination;

  if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
    log_file = fopen(file_path, "a");
    if (log_file == NULL) {
      fprintf(stderr, "Failed to open log file: %s : %s\n", file_path,
              strerror(errno));
      return NULL;
    }
  }

  log_info_(__FILE__, "Logger initialized with level %s and destination %s",
            log_level_to_string(level),
            log_destination == CONSOLE_ONLY ? "CONSOLE_ONLY"
            : log_destination == FILE_ONLY  ? "FILE_ONLY"
                                            : "CONSOLE_FILE");
  return log_file;
}

void log_trace_(const char *file, const char *message, ...) {
  if (log_level > TRACE) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(TRACE, file, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_debug_(const char *file, const char *message, ...) {
  if (log_level > DEBUG) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(DEBUG, file, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_info_(const char *file, const char *message, ...) {
  if (log_level > INFO) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(INFO, file, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_warn_(const char *file, const char *message, ...) {
  if (log_level > WARN) {
    return;
  }
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(WARN, file, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_error_(const char *file, const char *message, ...) {
  va_list args;
  va_list args_f, args_c;
  va_start(args_c, message);
  va_copy(args_f, args_c);
  log_message(ERROR, file, message, args_f, args_c);
  va_end(args_c);
  va_end(args_f);
}

void log_message(LOG_LEVEL level, const char *file, const char *message,
                 va_list args_f, va_list args_c) {
  time_t rawtime;
  struct tm *timeinfo;
  char time_str[20];
  pthread_t thread_id = pthread_self();
  struct session *session = get_session_for_thread();
  char *relative_file_path = (strstr(file, "src/") != NULL)
                                 ? strstr(file, "src/") + 4
                                 : strstr(file, "test/") + 5;

  // Get the current time
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // Format the time as YYYY-MM-DD HH:MM:SS
  strftime(time_str, sizeof(time_str), "%Y%m%d %H:%M:%S", timeinfo);

  pthread_mutex_lock(&log_mutex);

  if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
    if (log_file != NULL) {
      if (session != NULL) {
        fprintf(log_file, "%s %s (%s) [%lu-%d]: ", time_str, relative_file_path,
                log_level_to_string(level), (unsigned long)thread_id,
                session->id);
      } else {
        fprintf(log_file, "%s %s (%s) [%lu]: ", time_str, relative_file_path,
                log_level_to_string(level), (unsigned long)thread_id);
      }
      vfprintf(log_file, message, args_f);
      if (level == ERROR && errno != 0) {
        fprintf(log_file, ": %d %s\n", errno, strerror(errno));
      } else {
        fprintf(log_file, "\n");
      }
      fflush(log_file);
    }
  }
  if (log_destination == CONSOLE_ONLY || log_destination == CONSOLE_FILE) {
    if (session != NULL) {
      fprintf(stdout, "%s %s (%s) [%lu-%d]: ", time_str, relative_file_path,
              log_level_to_string(level), (unsigned long)thread_id,
              session->id);
    } else {
      fprintf(stdout, "%s %s (%s) [%lu]: ", time_str, relative_file_path,
              log_level_to_string(level), (unsigned long)thread_id);
    }
    vfprintf(stdout, message, args_c);
    if (level == ERROR && errno != 0) {
      fprintf(stdout, ": %d %s\n", errno, strerror(errno));
    } else {
      fprintf(stdout, "\n");
    }
  }

  pthread_mutex_unlock(&log_mutex);
}

// https://www.asciiart.eu/text-to-ascii-art
void log_logo() {
  fprintf(stdout,
          "__        ___           _   ____                 _             \n");
  fprintf(
      stdout,
      "\\ \\      / (_)_ __   __| | | __ ) _ __ ___  __ _| | _____ _ __ \n");
  fprintf(stdout, " \\ \\ /\\ / /| | '_ \\ / _` | |  _ \\| '__/ _ \\/ _` | |/ "
                  "/ _ \\ '__|\n");
  fprintf(stdout,
          "  \\ V  V / | | | | | (_| | | |_) | | |  __/ (_| |   <  __/ |   \n");
  fprintf(stdout, "   \\_/\\_/  |_|_| |_|\\__,_| |____/|_|  "
                  "\\___|\\__,_|_|\\_\\___|_|   \n");
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
