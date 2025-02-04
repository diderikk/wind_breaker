#include "logger.h"
#include "../data_structures/session.h"
#include <errno.h>
#include <openssl/err.h>
#include <stdarg.h>
#include <string.h>

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define RESET "\x1B[0m"

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

// Maybe remove coloring for "optimization"...
void log_to_destination(int log_destination, FILE *log_file,
                        const char *time_str, const char *relative_file_path,
                        const char *log_level, unsigned long thread_id,
                        struct session *session, const char *message,
                        va_list args_f, va_list args_c, int level) {
  fprintf(log_file, YEL "%-17s %-30s (%-5s) " RESET, time_str,
          relative_file_path, log_level_to_string(level));
  if (session != NULL) {
    fprintf(log_file, CYN "[%lu-%d]: " RESET, (unsigned long)thread_id,
            session->id);
  } else {
    fprintf(log_file, CYN "[%lu]: " RESET, (unsigned long)thread_id);
  }
  vfprintf(log_file, message, args_c);
  if (level == ERROR && (errno != 0 || ERR_peek_error() != 0)) {
    if (errno != 0)
      fprintf(log_file, RED ": %d %s\n" RESET, errno, strerror(errno));
    if (ERR_peek_error() != 0)
      fprintf(log_file, RED ": %s\n" RESET,
              ERR_error_string(ERR_get_error(), NULL));
  } else {
    fprintf(log_file, "\n");
  }
}

inline void log_message(LOG_LEVEL level, const char *file, const char *message,
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
      log_to_destination(log_destination, log_file, time_str,
                         relative_file_path, log_level_to_string(level),
                         (unsigned long)thread_id, session, message, args_f,
                         args_c, level);
      fflush(log_file);
    }
  }
  if (log_destination == CONSOLE_ONLY || log_destination == CONSOLE_FILE) {
    log_to_destination(log_destination, stdout, time_str, relative_file_path,
                       log_level_to_string(level), (unsigned long)thread_id,
                       session, message, args_f, args_c, level);
  }

  pthread_mutex_unlock(&log_mutex);
}

// https://www.asciiart.eu/text-to-ascii-art
void log_logo() {
  fprintf(stdout, RED
          "__        ___           _   ____                 _             \n");
  fprintf(
      stdout,
      "\\ \\      / (_)_ __   __| | | __ ) _ __ ___  __ _| | _____ _ __ \n");
  fprintf(stdout,
          GRN " \\ \\ /\\ / /| | '_ \\ / _` | |  _ \\| '__/ _ \\/ _` | |/ "
              "/ _ \\ '__|\n");
  fprintf(stdout,
          "  \\ V  V / | | | | | (_| | | |_) | | |  __/ (_| |   <  __/ |   \n");
  fprintf(stdout, BLU "   \\_/\\_/  |_|_| |_|\\__,_| |____/|_|  "
                      "\\___|\\__,_|_|\\_\\___|_|   \n" RESET);
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
