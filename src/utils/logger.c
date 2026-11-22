#include "logger.h"
#include "../data_structures/current_session.h"
#include "../static.h"
#include <errno.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_BUFFER_SIZE (1 * 1024 * 1024) // 1 MiB
#define TEMP_BUFFER_SIZE (4 * 1024)       // 4 KiB
#define BACKTRACE_SIZE 128
#define NO_ERRNO 0
#define WITH_BACKTRACE 1
#define WITHOUT_BACKTRACE 0
#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define RESET "\x1B[0m"

char *log_level_to_string(LOG_LEVEL level);
static void log_message(LOG_LEVEL level, const char *file, const char *message,
                        va_list args_c, int error_no, int with_backtrace);

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static LOG_LEVEL log_level = TRACE;
static LOG_DESTINATION log_destination = CONSOLE_ONLY;
// For file logging
static char *log_buffer = NULL;
static int log_buffer_offset = 0;
static FILE *log_file = NULL;
static long long file_offset = 0;

// TODO: Move from static buffers to thread buffers or on stack bufferes
// Logs to this buffer before file and/or console
static char temp_buffer[TEMP_BUFFER_SIZE];
static char error_buffer[TEMP_BUFFER_SIZE];
static unsigned int error_offset;
static char backtrace_buffer[TEMP_BUFFER_SIZE];
static unsigned int backtrace_offset;

static int system_page_size = 0;
static int file_offset_page_size_delta = 0;

static void reset_logger_state() {
  log_buffer = NULL;
  if (log_file != NULL) {
    fclose(log_file);
  }
  log_file = NULL;
  log_buffer_offset = 0;
  file_offset = 0;
  system_page_size = 0;
  file_offset_page_size_delta = 0;
  log_destination = CONSOLE_ONLY;
}

// mmap idea from UNDO C & C++ Logging for Debugging Concurrency video
int init_logger(LOG_LEVEL level, LOG_DESTINATION destination,
                const char *file_path) {
  // Prevent re-init on file
  if (log_file != NULL)
    return -1;
  system_page_size = (int)sysconf(_SC_PAGE_SIZE);
  if (system_page_size < 1) {
    fprintf(stderr, "Failed to get system page size: %d\n", system_page_size);
    return -1;
  }
  struct stat st;

  // Open file
  if (destination == FILE_ONLY || destination == CONSOLE_FILE) {
    log_file = fopen(file_path, "a+");
    if (log_file == NULL) {
      fprintf(stderr, "Failed to open log file: %s : %s\n", file_path,
              strerror(errno));
      return -1;
    }

    // Get file size
    int fd = fileno(log_file);
    if (fstat(fd, &st) == -1) {
      fprintf(stderr, "Failed to get file stats: %s : %s\n", file_path,
              strerror(errno));
      reset_logger_state();
      return -1;
    }
    file_offset = st.st_size;
    // mmap requires the file_offset to be a multiple of the system's page size
    file_offset_page_size_delta = file_offset % system_page_size;
    file_offset -= file_offset_page_size_delta;
    // Start writing delta into the buffer so we dont overwrite existing logs
    log_buffer_offset = file_offset_page_size_delta;

    // Truncate file to fit the new buffer size:
    // https://stackoverflow.com/a/7764277
    if (ftruncate(fd, file_offset + LOG_BUFFER_SIZE) == -1) {
      fprintf(stderr, "Failed to truncate log file: %s : %s\n", file_path,
              strerror(errno));
      reset_logger_state();
      return -1;
    }

    // Map file to memory
    log_buffer = mmap(NULL, LOG_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                      fd, file_offset);
    if (log_buffer == MAP_FAILED) {
      log_buffer = NULL;
      fprintf(stderr, "Failed to map log file: %s : %s\n", file_path,
              strerror(errno));
      ftruncate(fd, file_offset + log_buffer_offset);
      reset_logger_state();
      return -1;
    }
  }

  log_level = level;
  log_destination = destination;

  if (destination != FILE_ONLY)
    log_logo();
  log_info_(__FILE__, "Logger initialized with level %s and destination %s",
            log_level_to_string(level),
            destination == CONSOLE_ONLY ? "CONSOLE_ONLY"
            : destination == FILE_ONLY  ? "FILE_ONLY"
                                        : "CONSOLE_FILE");

  return 0;
}

void destroy_logger() {
  pthread_mutex_lock(&log_mutex);
  if (log_buffer != NULL) {
    msync(log_buffer, log_buffer_offset, MS_SYNC);
    if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
      munmap(log_buffer, LOG_BUFFER_SIZE);
      int fd = fileno(log_file);
      if (ftruncate(fd, file_offset + log_buffer_offset) == -1) {
        fprintf(stderr, "Failed to truncate log file: %s\n", strerror(errno));
      }
    }
  }
  reset_logger_state();
  pthread_mutex_unlock(&log_mutex);
}

void log_trace_(const char *file, const char *message, ...) {
  if (log_level > TRACE) {
    return;
  }
  va_list args_c;
  va_start(args_c, message);
  log_message(TRACE, file, message, args_c, NO_ERRNO, WITHOUT_BACKTRACE);
  va_end(args_c);
}

void log_debug_(const char *file, const char *message, ...) {
  if (log_level > DEBUG) {
    return;
  }
  va_list args_c;
  va_start(args_c, message);
  log_message(DEBUG, file, message, args_c, NO_ERRNO, WITHOUT_BACKTRACE);
  va_end(args_c);
}

void log_info_(const char *file, const char *message, ...) {
  if (log_level > INFO) {
    return;
  }
  va_list args_c;
  va_start(args_c, message);
  log_message(INFO, file, message, args_c, NO_ERRNO, WITHOUT_BACKTRACE);
  va_end(args_c);
}

void log_warn_(const char *file, const char *message, ...) {
  if (log_level > WARN) {
    return;
  }
  va_list args_c;
  va_start(args_c, message);
  log_message(WARN, file, message, args_c, NO_ERRNO, WITHOUT_BACKTRACE);
  va_end(args_c);
}

void log_error_(const char *file, const char *message, ...) {
  va_list args_c;
  va_start(args_c, message);
  log_message(ERROR, file, message, args_c, NO_ERRNO, WITHOUT_BACKTRACE);
  va_end(args_c);
}

void log_error_v_(const char *file, const char *message, va_list args_c) {
  log_message(ERROR, file, message, args_c, NO_ERRNO, WITHOUT_BACKTRACE);
}

void log_error_backtrace_(const char *file, const char *message, ...) {
  va_list args_c;
  va_start(args_c, message);
  log_message(ERROR, file, message, args_c, NO_ERRNO, WITH_BACKTRACE);
  va_end(args_c);
}

void log_errno_(const char *file, const char *message, ...) {
  int error_no = errno;
  va_list args_c;
  va_start(args_c, message);
  log_message(ERROR, file, message, args_c, error_no, WITH_BACKTRACE);
  va_end(args_c);
}

static inline int remap_log_buffer() {
  file_offset_page_size_delta = 0;
  if (log_destination == FILE_ONLY || log_destination == CONSOLE_FILE) {
    // Sync buffer to file
    msync(log_buffer, log_buffer_offset, MS_SYNC);
    munmap(log_buffer, LOG_BUFFER_SIZE);

    file_offset = file_offset + log_buffer_offset;
    // As in init, file_offset must be a multiple of system's page size
    file_offset_page_size_delta = file_offset % system_page_size;
    file_offset -= file_offset_page_size_delta;

    int fd = fileno(log_file);
    if (ftruncate(fd, file_offset + LOG_BUFFER_SIZE) == -1) {
      fprintf(stderr, "Failed to truncate log file: %s\n", strerror(errno));
      // Try to truncate it to remove null bytes
      ftruncate(fd, file_offset + file_offset_page_size_delta);
      return -1;
    }
    log_buffer = mmap(NULL, LOG_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                      fd, file_offset);
    if (log_buffer == MAP_FAILED) {
      fprintf(stderr, "Failed to map log file: %s\n", strerror(errno));
      // Try to truncate it to remove null bytes
      ftruncate(fd, file_offset + file_offset_page_size_delta);
      return -1;
    }
  }
  return file_offset_page_size_delta;
}

static inline unsigned int log_header(char *buffer, const char *time_str,
                                      const char *relative_file_path,
                                      unsigned long thread_id, int session_id,
                                      int level) {
  unsigned int offset = 0;
  offset += snprintf(buffer + offset, TEMP_BUFFER_SIZE - offset,
                     YEL "%-21s %-30s (%-5s) " RESET, time_str,
                     relative_file_path, log_level_to_string(level));
  if (session_id != -1) {
    offset +=
        snprintf(buffer + offset, TEMP_BUFFER_SIZE - offset,
                 CYN "[%lu-%d]: " RESET, (unsigned long)thread_id, session_id);
  } else {
    offset += snprintf(buffer + offset, TEMP_BUFFER_SIZE - offset,
                       CYN "[%lu]: " RESET, (unsigned long)thread_id);
  }
  return offset;
}

static inline unsigned int shorten(char *buffer) {
  char *last_four_bytes_buffer = buffer + (TEMP_BUFFER_SIZE - 4);
  memcpy(last_four_bytes_buffer, "...\n", 4);
  return TEMP_BUFFER_SIZE;
}

static inline unsigned int
log_to_backtrace_buffer(const char *time_str, const char *relative_file_path,
                        unsigned long thread_id, int session_id, int level) {
  unsigned int temp_offset = 0;
  backtrace_offset += log_header(backtrace_buffer, time_str, relative_file_path,
                                 thread_id, session_id, level);

  void *backtrace_stack_buffer[BACKTRACE_SIZE];
  unsigned int backtrace_size =
      backtrace(backtrace_stack_buffer, BACKTRACE_SIZE);
  char **backtrace_symbols_buffer =
      backtrace_symbols(backtrace_stack_buffer, backtrace_size);
  if (backtrace_symbols_buffer != NULL && backtrace_size > 0) {

    backtrace_offset +=
        snprintf(backtrace_buffer + backtrace_offset,
                 TEMP_BUFFER_SIZE - backtrace_offset, MAG "Backtrace:\n" RESET);
    for (unsigned int i = 0; i < backtrace_size; i++) {
      temp_offset =
          snprintf(backtrace_buffer + backtrace_offset,
                   TEMP_BUFFER_SIZE - backtrace_offset, MAG "  %d: %s\n" RESET,
                   i, backtrace_symbols_buffer[i]);
      if (temp_offset + backtrace_offset >= TEMP_BUFFER_SIZE) {
        backtrace_offset = shorten(backtrace_buffer);
        break;
      } else {
        backtrace_offset += temp_offset;
      }
    }
    free(backtrace_symbols_buffer);
  }

  // If not truncated
  if (backtrace_offset != TEMP_BUFFER_SIZE) {
    backtrace_buffer[backtrace_offset] = '\n';
    backtrace_offset++;
  }

  return backtrace_offset;
}

static inline unsigned int log_to_error_buffer(const char *time_str,
                                               const char *relative_file_path,
                                               unsigned long thread_id,
                                               int session_id, int level,
                                               int error_no) {
  unsigned int temp_offset = 0;
  error_offset += log_header(error_buffer, time_str, relative_file_path,
                             thread_id, session_id, level);

  if (error_no != 0) {
    temp_offset =
        snprintf(error_buffer + error_offset, TEMP_BUFFER_SIZE - error_offset,
                 RED ": %d %s" RESET, error_no, strerror(error_no));

    if (temp_offset + error_offset >= TEMP_BUFFER_SIZE) {
      error_offset = shorten(error_buffer);
    } else
      error_offset += temp_offset;
  }

  if (error_offset != TEMP_BUFFER_SIZE) {
    error_buffer[error_offset] = '\n';
    error_offset++;
  }

  return error_offset;
}

// Maybe remove coloring for "optimization"...
static inline unsigned int
log_to_buffer(const char *time_str, const char *relative_file_path,
              unsigned long thread_id, int session_id, const char *message,
              va_list args_c, int level, int error_no, int with_backtrace) {
  unsigned int offset = 0;
  unsigned int temp_offset = 0;
  offset += log_header(temp_buffer, time_str, relative_file_path, thread_id,
                       session_id, level);

  temp_offset = vsnprintf(temp_buffer + offset, TEMP_BUFFER_SIZE - offset,
                          message, args_c);
  if (temp_offset + offset >= TEMP_BUFFER_SIZE) {
    offset = shorten(temp_buffer);
  } else {
    offset += temp_offset;
  }
  if (offset != TEMP_BUFFER_SIZE) {
    temp_buffer[offset] = '\n';
    offset++;
  }

  // Error handling, always reset error offset
  error_offset = 0;
  backtrace_offset = 0;
  if (with_backtrace == WITH_BACKTRACE) {
    log_to_backtrace_buffer(time_str, relative_file_path, thread_id, session_id,
                            level);
  }
  if (error_no > 0) {
    log_to_error_buffer(time_str, relative_file_path, thread_id, session_id,
                        level, error_no);
  }

  return offset;
}

static void log_message(LOG_LEVEL level, const char *file, const char *message,
                        va_list args_c, int error_no, int with_backtrace) {
  struct tm timeinfo;
  char temp[20], time_str[24];
  pthread_t thread_id = pthread_self();
  int session_id = get_current_session();
  const char *relative_file_path =
      (strstr(file, "src/") != NULL) ? strstr(file, "src/") + 4
      : (strstr(file, "test/"))      ? strstr(file, "test/") + 5
                                     : file;

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  localtime_r(&ts.tv_sec, &timeinfo);

  strftime(temp, sizeof(temp), "%Y%m%d %H:%M:%S", &timeinfo);
  snprintf(time_str, sizeof(time_str), "%s.%03ld", temp, ts.tv_nsec / 1000000);

  pthread_mutex_lock(&log_mutex);

  unsigned int offset = 0;
  offset = log_to_buffer(time_str, relative_file_path, (unsigned long)thread_id,
                         session_id, message, args_c, level, error_no,
                         with_backtrace);
  unsigned int total_offset = offset + error_offset + backtrace_offset;

  // Log to file
  if (log_buffer != NULL) {
    int remap_offset = 0;
    if (log_buffer_offset + total_offset > LOG_BUFFER_SIZE) {
      remap_offset = remap_log_buffer();
      if (remap_offset < 0) {
        fprintf(stderr,
                "Failed to remap log buffer, going to CONSOLE_ONLY...\n");
        // Resetting to CONSOLE_ONLY
        reset_logger_state();
      } else {
        log_buffer_offset = remap_offset;
      }
    }
    if (remap_offset >= 0) {
      memcpy(log_buffer + log_buffer_offset, temp_buffer, offset);
      log_buffer_offset += offset;
      if (error_offset > 0) {
        memcpy(log_buffer + log_buffer_offset, error_buffer, error_offset);
        log_buffer_offset += error_offset;
      }
      if (backtrace_offset > 0) {
        memcpy(log_buffer + log_buffer_offset, backtrace_buffer,
               backtrace_offset);
        log_buffer_offset += backtrace_offset;
      }
    }
  }

  // Log to console
  if (log_destination == CONSOLE_ONLY || log_destination == CONSOLE_FILE) {
    printf("%.*s%.*s%.*s", (int)offset, temp_buffer, (int)error_offset,
           error_buffer, (int)backtrace_offset, backtrace_buffer);
  }

  pthread_mutex_unlock(&log_mutex);
}

// https://www.asciiart.eu/text-to-ascii-art
void log_logo() {
  printf(RED
         "__        ___           _   ____                 _             \n");
  printf("\\ \\      / (_)_ __   __| | | __ ) _ __ ___  __ _| | _____ _ __ \n");
  printf(GRN " \\ \\ /\\ / /| | '_ \\ / _` | |  _ \\| '__/ _ \\/ _` | |/ "
             "/ _ \\ '__|\n");
  printf("  \\ V  V / | | | | | (_| | | |_) | | |  __/ (_| |   <  __/ |   \n");
  printf(BLU "   \\_/\\_/  |_|_| |_|\\__,_| |____/|_|  "
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
