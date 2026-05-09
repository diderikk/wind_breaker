#include "logger.h"
#include "../data_structures/current_session.h"
#include "../static.h"
#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <openssl/err.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_BUFFER_SIZE 4 * 1024 * 1024 // 4 MiB
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
static int log_buffer_offset = 0;
static char *log_buffer = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// mmap idea from UNDO C & C++ Logging for Debugging Concurrency video
FILE *init_logger(LOG_LEVEL level, LOG_DESTINATION destination,
                  const char *file_path) {
  log_level = level;
  log_destination = destination;
  struct stat st;

  // Open file
  if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
    log_file = fopen(file_path, "a+");
    if (log_file == NULL) {
      fprintf(stderr, "Failed to open log file: %s : %s\n", file_path,
              strerror(errno));
      return NULL;
    }

    // Get file size
    int fd = fileno(log_file);
    if (fstat(fd, &st) == -1) {
      fprintf(stderr, "Failed to get file stats: %s : %s\n", file_path,
              strerror(errno));
      return NULL;
    }
    off_t offset = st.st_size;

    // Truncate file to fit the new buffer size:
    // https://stackoverflow.com/a/7764277
    if (ftruncate(fd, offset + LOG_BUFFER_SIZE) == -1) {
      fprintf(stderr, "Failed to truncate log file: %s : %s\n", file_path,
              strerror(errno));
      return NULL;
    }

    // Map file to memory
    log_buffer = mmap(NULL, LOG_BUFFER_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED_VALIDATE, fd, offset);
    if (log_buffer == MAP_FAILED) {
      fprintf(stderr, "Failed to map log file: %s : %s\n", file_path,
              strerror(errno));
      return NULL;
    }
  } else {
    log_buffer = malloc(LOG_BUFFER_SIZE);
  }

  log_logo();
  log_info_(__FILE__, "Logger initialized with level %s and destination %s",
            log_level_to_string(level),
            log_destination == CONSOLE_ONLY ? "CONSOLE_ONLY"
            : log_destination == FILE_ONLY  ? "FILE_ONLY"
                                            : "CONSOLE_FILE");
  return log_file;
}

void destroy_logger() {
  msync(log_buffer, log_buffer_offset, MS_SYNC);
  if (log_file != NULL) {
    fclose(log_file);
  }
  log_file = NULL;
  if (log_buffer != NULL) {
    if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
      munmap(log_buffer, LOG_BUFFER_SIZE);
    } else {
      free(log_buffer);
    }
  }
  log_buffer = NULL;
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

static inline int remap_log_buffer() {
  if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
    // Sync buffer to file
    msync(log_buffer, log_buffer_offset, MS_SYNC);
    munmap(log_buffer, LOG_BUFFER_SIZE);

    // Get file new size and sets pointer to the end of file
    // (no offset needed in mmap)
    if (fseek(log_file, 0, SEEK_END) < 0) {
      fprintf(stderr, "Failed to seek to end of log file: %s\n",
              strerror(errno));
      return -1;
    }
    int size = ftell(log_file);
    if (size < 0) {
      fprintf(stderr, "Failed to get file size: %s\n", strerror(errno));
      return -1;
    }

    int fd = fileno(log_file);
    if (ftruncate(fd, size + LOG_BUFFER_SIZE) == -1) {
      fprintf(stderr, "Failed to truncate log file: %s\n", strerror(errno));
      return -1;
    }
    log_buffer = mmap(NULL, LOG_BUFFER_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED_VALIDATE, fd, size);
    if (log_buffer == MAP_FAILED) {
      fprintf(stderr, "Failed to map log file: %s\n", strerror(errno));
      return -1;
    }
  }
  printf("Remapped log buffer\n");
  return 0;
}

// Maybe remove coloring for "optimization"...
static inline unsigned int
log_to_buffer(int log_destination, const char *time_str,
              const char *relative_file_path, const char *log_level,
              unsigned long thread_id, int session_id, const char *message,
              va_list args_f, va_list args_c, int level) {
  unsigned int offset = 0;
  offset += sprintf(log_buffer + (log_buffer_offset + offset),
                    YEL "%-17s %-30s (%-5s) " RESET, time_str,
                    relative_file_path, log_level_to_string(level));
  if (session_id != -1) {
    offset +=
        sprintf(log_buffer + (log_buffer_offset + offset),
                CYN "[%lu-%d]: " RESET, (unsigned long)thread_id, session_id);
  } else {
    offset += sprintf(log_buffer + (log_buffer_offset + offset),
                      CYN "[%lu]: " RESET, (unsigned long)thread_id);
  }
  offset +=
      vsprintf(log_buffer + (log_buffer_offset + offset), message, args_c);

  // Error handling
  if (level == ERROR && (errno != 0 || ERR_peek_error() != 0)) {
    void *backtrace_buffer[BACKTRACE_SIZE];
    unsigned int backtrace_size = backtrace(backtrace_buffer, BACKTRACE_SIZE);
    char **backtrace_symbols_buffer =
        backtrace_symbols(backtrace_buffer, backtrace_size);
    if (errno != 0)
      offset += sprintf(log_buffer + (log_buffer_offset + offset),
                        RED ": %d %s\n" RESET, errno, strerror(errno));
    if (ERR_peek_error() != 0)
      offset +=
          sprintf(log_buffer + (log_buffer_offset + offset), RED ": %s\n" RESET,
                  ERR_error_string(ERR_get_error(), NULL));
    if (backtrace_symbols_buffer != NULL && backtrace_size > 0) {
      offset += sprintf(log_buffer + (log_buffer_offset + offset),
                        MAG "Backtrace:\n" RESET);
      for (unsigned int i = 0; i < backtrace_size; i++) {
        offset +=
            sprintf(log_buffer + (log_buffer_offset + offset),
                    MAG "  %d: %s\n" RESET, i, backtrace_symbols_buffer[i]);
      }
      free(backtrace_symbols_buffer);
    }
  } else {
    offset += sprintf(log_buffer + (log_buffer_offset + offset), "\n");
  }

  return offset;
}

inline void log_message(LOG_LEVEL level, const char *file, const char *message,
                        va_list args_f, va_list args_c) {
  time_t rawtime;
  struct tm *timeinfo;
  char time_str[20];
  pthread_t thread_id = pthread_self();
  int session_id = get_current_session();
  char *relative_file_path = (strstr(file, "src/") != NULL)
                                 ? strstr(file, "src/") + 4
                                 : strstr(file, "test/") + 5;

  // Get the current time
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // Format the time as YYYYMMDD HH:MM:SS
  strftime(time_str, sizeof(time_str), "%Y%m%d %H:%M:%S", timeinfo);

  pthread_mutex_lock(&log_mutex);

  int written = 0;
  if (log_buffer != NULL) {
    written =
        log_to_buffer(log_destination, time_str, relative_file_path,
                      log_level_to_string(level), (unsigned long)thread_id,
                      session_id, message, args_f, args_c, level);
    if (log_destination == CONSOLE_ONLY || log_destination == CONSOLE_FILE) {
      printf("%s", log_buffer + log_buffer_offset);
    }
    log_buffer_offset += written;

    if (log_buffer_offset > LOG_BUFFER_SIZE - 1024 * 1024) {
      assert(remap_log_buffer() >= 0);
      log_buffer_offset = 0;
    }
  }

  pthread_mutex_unlock(&log_mutex);
}

// https://www.asciiart.eu/text-to-ascii-art
void log_logo() {
  int offset = 0;
  offset += sprintf(
      log_buffer + (offset + log_buffer_offset),
      RED "__        ___           _   ____                 _             \n");
  offset += sprintf(
      log_buffer + (offset + log_buffer_offset),
      "\\ \\      / (_)_ __   __| | | __ ) _ __ ___  __ _| | _____ _ __ \n");
  offset +=
      sprintf(log_buffer + (offset + log_buffer_offset),
              GRN " \\ \\ /\\ / /| | '_ \\ / _` | |  _ \\| '__/ _ \\/ _` | |/ "
                  "/ _ \\ '__|\n");
  offset += sprintf(
      log_buffer + (offset + log_buffer_offset),
      "  \\ V  V / | | | | | (_| | | |_) | | |  __/ (_| |   <  __/ |   \n");
  offset += sprintf(log_buffer + (offset + log_buffer_offset),
                    BLU "   \\_/\\_/  |_|_| |_|\\__,_| |____/|_|  "
                        "\\___|\\__,_|_|\\_\\___|_|   \n" RESET);

  printf("%s", log_buffer + log_buffer_offset);
  log_buffer_offset += offset;
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
