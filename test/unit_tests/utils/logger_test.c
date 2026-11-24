#include "../../../src/utils/assert2.h"
#include "../../../src/utils/logger.h"
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Files every test works with. Tests run one after another, so fixed names are
// fine. The child process writes its stdout/stderr to the console/stderr files.
#define TEST_LOG_PATH "/tmp/wb_logger_test.log"
#define TEST_CONSOLE_PATH "/tmp/wb_logger_test_console.txt"
#define TEST_STDERR_PATH "/tmp/wb_logger_test_stderr.txt"
// A directory that does not exist, so fopen() on it fails.
#define TEST_BAD_PATH "/wb_logger_test_missing_dir/x.log"

// Mirrors LOG_BUFFER_SIZE in logger.c (it is not exposed in the header). If
// you change the buffer size there, change it here too: the remap tests depend
// on it.
#define LOGGER_TEST_BUFFER_SIZE (1 * 1024 * 1024)

// Pads every line to ~210 bytes (with the header), so ~60000 lines cross many
// remaps (a remap happens roughly every 1 MiB).
#define LOGGER_TEST_PADDING                                                    \
  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" \
  "xxxxxxxxxx"
#define LOGGER_TEST_MANY_LINES 60000
// Enough to cross the first remap (~4900 lines per MiB) with some left over.
// Kept small because the console file shares the file size limit that the
// remap failure scenario sets.
#define LOGGER_TEST_REMAP_LINES 6000

// Message lengths around the per-line buffer size (4 KiB), so both "exactly
// fits" and "one byte too long" are hit whatever the header length is.
#define LOGGER_TEST_SWEEP_FIRST 3800
#define LOGGER_TEST_SWEEP_LAST 4300

static int logger_test_count = 0;
static int logger_start_case(void *(*func)(void *), const char *name);

/* ------------------------------------------------------------------------ */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------ */

__attribute__((format(printf, 2, 3))) static void
logger_check(int condition, const char *format, ...) {
  if (!condition) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "  FAILED: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
  }
  assert(condition);
}

static void logger_remove_test_files(void) {
  unlink(TEST_LOG_PATH);
  unlink(TEST_CONSOLE_PATH);
  unlink(TEST_STDERR_PATH);
}

// Reads the whole file. The caller frees the buffer.
static char *logger_read_file(const char *path, long *size) {
  FILE *f = fopen(path, "rb");
  logger_check(f != NULL, "%s should exist", path);
  fseek(f, 0, SEEK_END);
  *size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buffer = malloc(*size > 0 ? *size : 1);
  logger_check(fread(buffer, 1, *size, f) == (size_t)*size,
               "%s should be readable", path);
  fclose(f);
  return buffer;
}

static int logger_file_contains(const char *path, const char *needle) {
  long size = 0;
  char *buffer = logger_read_file(path, &size);
  int found = memmem(buffer, size, needle, strlen(needle)) != NULL;
  free(buffer);
  return found;
}

static long logger_count_nul_bytes(const char *buffer, long size) {
  long count = 0;
  for (long i = 0; i < size; i++) {
    if (buffer[i] == '\0')
      count++;
  }
  return count;
}

static long logger_count_occurrences(const char *buffer, long size,
                                     const char *needle) {
  long count = 0;
  size_t length = strlen(needle);
  const char *cursor = buffer;
  const char *end = buffer + size;
  while ((cursor = memmem(cursor, end - cursor, needle, length)) != NULL) {
    count++;
    cursor += length;
  }
  return count;
}

// Number of open file descriptors in this process (used to detect a leaked
// FILE* after a failed init).
static int logger_count_open_fds(void) {
  DIR *dir = opendir("/dev/fd");
  if (dir == NULL)
    return -1;
  int count = 0;
  while (readdir(dir) != NULL)
    count++;
  closedir(dir);
  return count;
}

// Logs "LINE 000000 xxx...", "LINE 000001 xxx..." and so on, for [from, to).
static void logger_log_lines(int from, int to) {
  for (int i = from; i < to; i++) {
    log_info("LINE %06d " LOGGER_TEST_PADDING, i);
  }
}

// Checks that the "LINE nnnnnn" markers in the buffer are exactly
// first, first + 1, ..., last - 1: in order, none lost, none duplicated.
static void logger_expect_lines(const char *buffer, long size, int first,
                                int last) {
  int expected = first;
  for (long i = 0; i + 11 <= size; i++) {
    if (memcmp(buffer + i, "LINE ", 5) != 0)
      continue;
    int number = atoi(buffer + i + 5);
    logger_check(number == expected,
                 "expected LINE %06d but found LINE %06d at byte %ld", expected,
                 number, i);
    expected++;
  }
  logger_check(expected == last, "expected %d lines, found %d", last - first,
               expected - first);
}

// Logger state is global and the suite already initialised it in main(), so
// every scenario runs in a forked child. That keeps the rest of the suite
// untouched and lets us see a crash as an exit status.
//
// The child's stdout and stderr go to TEST_CONSOLE_PATH and TEST_STDERR_PATH.
// The test fails if the child crashes or exits with a non-zero status.
static void logger_run_in_child(void (*scenario)(void)) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  logger_check(pid >= 0, "fork should work");

  if (pid == 0) {
    if (freopen(TEST_CONSOLE_PATH, "w", stdout) == NULL ||
        freopen(TEST_STDERR_PATH, "w", stderr) == NULL)
      _exit(2);
    setvbuf(stderr, NULL, _IONBF, 0);
    alarm(60); // a hang (e.g. a deadlock) turns into SIGALRM instead of a stall
    destroy_logger(); // drop the console logger inherited from the parent
    scenario();
    fflush(stdout); // _exit() does not flush stdio buffers
    _exit(0);
  }

  int status = 0;
  waitpid(pid, &status, 0);
  if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
    long size = 0;
    char *child_stderr = logger_read_file(TEST_STDERR_PATH, &size);
    fprintf(stderr, "  child stderr:\n%.*s", (int)size, child_stderr);
    free(child_stderr);
  }
  logger_check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
               "scenario should exit cleanly (wait status %d)", status);
}

/* ------------------------------------------------------------------------ */
/* File output                                                              */
/* ------------------------------------------------------------------------ */

static void logger_scenario_few_lines(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_lines(0, 10);
  destroy_logger();
}

// destroy_logger used to leave the unused part of the mapping behind as zeros.
void *logger_file_has_no_zero_tail_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_few_lines);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_check(size > 0, "file should contain the logged lines");
  logger_check(size < LOGGER_TEST_BUFFER_SIZE / 16,
               "file should be trimmed to the written bytes, but is %ld bytes",
               size);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_many_lines(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_lines(0, LOGGER_TEST_MANY_LINES);
  destroy_logger();
}

// After a remap, file_offset must follow the new mapping. Otherwise a later
// chunk is mapped on top of earlier logs, or leaves ~1 MiB of zeros between
// chunks, and destroy_logger truncates away everything after the first chunk.
void *logger_file_keeps_all_lines_across_remaps_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_many_lines);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_check(size > 3 * LOGGER_TEST_BUFFER_SIZE,
               "test should have written enough to trigger several remaps");
  logger_expect_lines(buffer, size, 0, LOGGER_TEST_MANY_LINES);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero gaps between remaps");

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_first_run(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_lines(0, 100);
  destroy_logger();
}

static void logger_scenario_second_run(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_lines(100, 200);
  destroy_logger();
}

// A second run appends to the file: the first run's lines must still be there,
// with no zero tail between the runs.
void *logger_file_appends_across_runs_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_first_run);
  logger_run_in_child(logger_scenario_second_run);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_expect_lines(buffer, size, 0, 200);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

/* ------------------------------------------------------------------------ */
/* Console output                                                           */
/* ------------------------------------------------------------------------ */

static void logger_scenario_console_only(void) {
  logger_check(init_logger(TRACE, CONSOLE_ONLY, NULL) == 0,
               "init_logger should succeed");
  logger_log_lines(0, 5);
  destroy_logger();
}

// With no log file, destroy_logger used to call fileno(NULL).
void *logger_console_only_destroy_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_console_only);

  logger_check(logger_file_contains(TEST_CONSOLE_PATH, "LINE 000004"),
               "lines should reach the console");

  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_console_and_file(void) {
  logger_check(init_logger(TRACE, CONSOLE_FILE, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_lines(0, 5);
  destroy_logger();
}

// CONSOLE_FILE writes every line to both places.
void *logger_console_and_file_destination_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_console_and_file);

  long size = 0, console_size = 0;
  char *file = logger_read_file(TEST_LOG_PATH, &size);
  char *console = logger_read_file(TEST_CONSOLE_PATH, &console_size);
  logger_expect_lines(file, size, 0, 5);
  logger_expect_lines(console, console_size, 0, 5);

  free(console);
  free(file);
  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_log_after_destroy(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_lines(0, 3);
  destroy_logger();
  log_info("AFTER DESTROY");
  log_error("ERROR AFTER DESTROY");
}

// Logging after destroy_logger() falls back to the console. It must not touch
// the unmapped buffer or the closed file.
void *logger_log_after_destroy_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_log_after_destroy);

  logger_check(logger_file_contains(TEST_CONSOLE_PATH, "AFTER DESTROY"),
               "logging after destroy should reach the console");
  logger_check(logger_file_contains(TEST_CONSOLE_PATH, "ERROR AFTER DESTROY"),
               "errors after destroy should reach the console");
  logger_check(!logger_file_contains(TEST_LOG_PATH, "AFTER DESTROY"),
               "logging after destroy must not write to the log file");
  logger_check(logger_file_contains(TEST_LOG_PATH, "LINE 000002"),
               "lines logged before destroy should be in the file");

  logger_remove_test_files();
  return NULL;
}

/* ------------------------------------------------------------------------ */
/* Levels, errno and backtraces                                             */
/* ------------------------------------------------------------------------ */

static void logger_scenario_level_filter(void) {
  logger_check(init_logger(WARN, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  log_trace("TRACE MSG");
  log_debug("DEBUG MSG");
  log_info("INFO MSG");
  log_warn("WARN MSG");
  log_error("ERROR MSG");
  destroy_logger();
}

// Only messages at or above the configured level may be written.
void *logger_level_filter_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_level_filter);

  logger_check(!logger_file_contains(TEST_LOG_PATH, "TRACE MSG"),
               "TRACE should be filtered out at level WARN");
  logger_check(!logger_file_contains(TEST_LOG_PATH, "DEBUG MSG"),
               "DEBUG should be filtered out at level WARN");
  logger_check(!logger_file_contains(TEST_LOG_PATH, "INFO MSG"),
               "INFO should be filtered out at level WARN");
  logger_check(logger_file_contains(TEST_LOG_PATH, "WARN MSG"),
               "WARN should be logged at level WARN");
  logger_check(logger_file_contains(TEST_LOG_PATH, "ERROR MSG"),
               "ERROR should be logged at level WARN");

  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_errno(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  errno = ENOENT;
  log_error("PLAIN ERROR MSG"); // stale errno: must not be reported
  errno = ENOENT;
  log_errno("ERRNO MSG");
  errno = 0;
  log_errno("NO ERRNO MSG"); // nothing to report
  logger_log_lines(0, 3);
  destroy_logger();
}

// log_errno() reports errno (and a backtrace). log_error() must not pick up a
// stale errno, and log_errno() with errno == 0 has nothing to report.
void *logger_errno_is_only_reported_by_log_errno_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_errno);

  const char *enoent = strerror(ENOENT);
  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);

  logger_check(logger_file_contains(TEST_LOG_PATH, "PLAIN ERROR MSG"),
               "log_error should be logged");
  logger_check(logger_file_contains(TEST_LOG_PATH, "ERRNO MSG"),
               "log_errno should be logged");
  logger_check(logger_file_contains(TEST_LOG_PATH, "Backtrace:"),
               "log_errno should include a backtrace");
  logger_check(logger_count_occurrences(buffer, size, enoent) == 1,
               "only the log_errno call with errno set should report \"%s\"",
               enoent);
  logger_expect_lines(buffer, size, 0, 3);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_error_backtrace(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  errno = ENOENT; // must not be reported by log_error_backtrace()
  log_error_backtrace("BACKTRACE MSG");
  logger_log_lines(0, 3);
  destroy_logger();
}

void *logger_error_backtrace_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_error_backtrace);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_check(logger_file_contains(TEST_LOG_PATH, "BACKTRACE MSG"),
               "message should be logged");
  logger_check(logger_file_contains(TEST_LOG_PATH, "Backtrace:"),
               "log_error_backtrace should include a backtrace");
  logger_check(!logger_file_contains(TEST_LOG_PATH, strerror(ENOENT)),
               "log_error_backtrace should not report errno");
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

// Deep recursion so the backtrace has the maximum number of frames and cannot
// fit in the per-line buffer.
static volatile int logger_recursion_sink = 0;
__attribute__((noinline)) static int logger_log_from_deep_stack(int depth) {
  if (depth == 0) {
    log_error_backtrace("DEEP BACKTRACE MSG");
    return 0;
  }
  logger_recursion_sink += logger_log_from_deep_stack(depth - 1);
  return logger_recursion_sink + 1;
}

static void logger_scenario_deep_backtrace(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_from_deep_stack(200);
  logger_log_lines(0, 3);
  destroy_logger();
}

// A backtrace that does not fit in the per-line buffer is cut with "...", and
// the next log line must still start cleanly.
void *logger_deep_backtrace_is_truncated_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_deep_backtrace);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_check(logger_file_contains(TEST_LOG_PATH, "DEEP BACKTRACE MSG"),
               "message should be logged");
  logger_check(logger_file_contains(TEST_LOG_PATH, "...\n"),
               "an oversized backtrace should end with ...\\n");
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

static void logger_log_error_via_va_list(const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_error_v(format, args);
  va_end(args);
}

static void logger_scenario_error_v(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_error_via_va_list("VA_LIST MSG %d %s", 42, "forty-two");
  logger_log_lines(0, 3);
  destroy_logger();
}

void *logger_error_v_formats_arguments_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_error_v);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_check(logger_file_contains(TEST_LOG_PATH, "VA_LIST MSG 42 forty-two"),
               "log_error_v should format the va_list arguments");
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

/* ------------------------------------------------------------------------ */
/* Line truncation                                                          */
/* ------------------------------------------------------------------------ */

static void logger_scenario_long_line(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  // Far longer than the per-line temp buffer
  char long_message[20000];
  memset(long_message, 'y', sizeof(long_message) - 1);
  long_message[sizeof(long_message) - 1] = '\0';
  log_info("%s", long_message);
  logger_log_lines(0, 3);
  destroy_logger();
}

// A message longer than the temp buffer must be cut short, still end in a
// newline, not leak into the next line, and not corrupt neighbouring memory.
void *logger_long_line_is_truncated_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_long_line);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_check(size < 20000, "the long message should have been truncated");
  logger_check(logger_file_contains(TEST_LOG_PATH, "...\n"),
               "a truncated line should end with ...\\n");
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "truncating a long line must not leave zero bytes in the file");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");
  // The lines after the long one must be intact and start on their own line.
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_length_sweep(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  char message[LOGGER_TEST_SWEEP_LAST + 1];
  for (int length = LOGGER_TEST_SWEEP_FIRST; length <= LOGGER_TEST_SWEEP_LAST;
       length++) {
    memset(message, 'z', length);
    message[length] = '\0';
    log_info("%s", message);
  }
  destroy_logger();
}

// Whatever the message length is (just under, exactly at, or over the temp
// buffer size), every log call must produce exactly one newline-terminated
// line and no zero bytes.
void *logger_line_length_boundaries_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_length_sweep);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  // One line per message, plus the "Logger initialized" line
  long expected_lines =
      LOGGER_TEST_SWEEP_LAST - LOGGER_TEST_SWEEP_FIRST + 1 + 1;
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "no line length may produce zero bytes");
  logger_check(logger_count_occurrences(buffer, size, "\n") == expected_lines,
               "expected exactly %ld newlines, one per message",
               expected_lines);
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

/* ------------------------------------------------------------------------ */
/* Remap failure                                                            */
/* ------------------------------------------------------------------------ */

// Forces the first remap to fail: the file size limit allows the initial 1 MiB
// mapping but not the next one (ftruncate fails with EFBIG).
static void logger_scenario_remap_failure(void) {
  signal(SIGXFSZ, SIG_IGN);
  struct rlimit limit = {.rlim_cur = LOGGER_TEST_BUFFER_SIZE * 3 / 2,
                         .rlim_max = LOGGER_TEST_BUFFER_SIZE * 3 / 2};
  logger_check(setrlimit(RLIMIT_FSIZE, &limit) == 0,
               "setrlimit should succeed");

  logger_check(init_logger(INFO, FILE_ONLY, TEST_LOG_PATH) == 0,
               "init_logger should succeed");
  logger_log_lines(0, LOGGER_TEST_REMAP_LINES);
  destroy_logger();
}

// A failed remap must not crash, hang, or corrupt what was already written.
// The logger is expected to fall back to console-only logging.
void *logger_remap_failure_falls_back_to_console_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_remap_failure);

  // Guard against a vacuous pass: the failure path must really have run (the
  // file size limit in the scenario depends on LOGGER_TEST_BUFFER_SIZE).
  logger_check(
      logger_file_contains(TEST_STDERR_PATH, "Failed to remap log buffer"),
      "the scenario should have triggered a remap failure");

  // Everything logged before the failure must still be in the file.
  long size = 0;
  char *file = logger_read_file(TEST_LOG_PATH, &size);
  long lines_in_file = logger_count_occurrences(file, size, "LINE ");
  logger_check(size <= LOGGER_TEST_BUFFER_SIZE * 3 / 2,
               "the file should not grow past the size limit");
  logger_check(lines_in_file > 1000 && lines_in_file < LOGGER_TEST_REMAP_LINES,
               "the failure should happen part way through the run, but %ld "
               "lines are in the file",
               lines_in_file);
  logger_expect_lines(file, size, 0, lines_in_file);
  logger_check(file[size - 1] == '\n',
               "file should end at the last line written before the failure");
  logger_check(logger_count_nul_bytes(file, size) == 0,
               "file should not contain any zero bytes");

  // After the failure the logger keeps working, on the console.
  char last_line[16];
  snprintf(last_line, sizeof(last_line), "LINE %06d",
           LOGGER_TEST_REMAP_LINES - 1);
  logger_check(logger_file_contains(TEST_CONSOLE_PATH, last_line),
               "lines logged after the failure should reach the console");

  free(file);
  logger_remove_test_files();
  return NULL;
}

// The line that triggers a failed remap must not vanish: it is neither in the
// file (the mapping is gone) nor, for FILE_ONLY, on the console. Every line
// should be in the file or on the console.
// KNOWN BUG until log_message() writes the current entry to the console when
// the fallback happens.
void *logger_remap_failure_does_not_lose_a_line_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_remap_failure);

  long size = 0, console_size = 0;
  char *file = logger_read_file(TEST_LOG_PATH, &size);
  char *console = logger_read_file(TEST_CONSOLE_PATH, &console_size);

  // The file holds lines 0..n-1, so the console must hold n..last, with no gap.
  int lines_in_file = (int)logger_count_occurrences(file, size, "LINE ");
  logger_expect_lines(file, size, 0, lines_in_file);
  logger_expect_lines(console, console_size, lines_in_file,
                      LOGGER_TEST_REMAP_LINES);

  free(console);
  free(file);
  logger_remove_test_files();
  return NULL;
}

/* ------------------------------------------------------------------------ */
/* init_logger failures                                                     */
/* ------------------------------------------------------------------------ */

static void logger_scenario_open_failure(void) {
  // ERROR + FILE_ONLY must NOT be applied when init fails: otherwise the INFO
  // line below would be filtered out or sent nowhere.
  logger_check(init_logger(ERROR, FILE_ONLY, TEST_BAD_PATH) != 0,
               "init_logger should fail when the log file cannot be opened");
  log_info("AFTER FAILED INIT");
  destroy_logger(); // must be safe on a logger that never finished init
  log_info("AFTER DESTROY");
}

// init_logger reports failure (non-zero) when the log file cannot be opened,
// and the logger keeps working as a console logger afterwards: the requested
// level/destination are not applied, nothing crashes, destroy_logger is safe.
void *logger_init_open_failure_falls_back_to_console_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_open_failure);

  logger_check(logger_file_contains(TEST_CONSOLE_PATH, "AFTER FAILED INIT"),
               "after a failed init the logger should still log to the "
               "console, at the previous level");
  logger_check(logger_file_contains(TEST_CONSOLE_PATH, "AFTER DESTROY"),
               "logging after destroy_logger should not crash");
  logger_check(!logger_file_contains(TEST_CONSOLE_PATH, "Logger initialized"),
               "a failed init must not report success");

  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_truncate_failure(void) {
  // Makes ftruncate() of the new log file fail, which is a failure after the
  // file was already opened.
  signal(SIGXFSZ, SIG_IGN);
  struct rlimit limit = {.rlim_cur = 1024, .rlim_max = 1024};
  logger_check(setrlimit(RLIMIT_FSIZE, &limit) == 0,
               "setrlimit should succeed");

  int fds_before = logger_count_open_fds();
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) != 0,
               "init_logger should fail when the file cannot be extended");
  logger_check(logger_count_open_fds() == fds_before,
               "the FILE* opened by init_logger should be closed on failure");
  log_info("AFTER FAILED INIT");
  log_error("ERROR AFTER FAILED INIT");
  destroy_logger();
}

// Failure after the file was opened (ftruncate fails): init reports failure,
// does not crash on later log calls, and leaves the file as it found it.
void *logger_init_truncate_failure_leaves_clean_state_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_truncate_failure);

  logger_check(
      logger_file_contains(TEST_CONSOLE_PATH, "ERROR AFTER FAILED INIT"),
      "errors logged after a failed init should reach the console");

  long size = 0;
  char *file = logger_read_file(TEST_LOG_PATH, &size);
  logger_check(size == 0,
               "a failed init must not leave the file extended with zeros or "
               "write log lines into it, but it is %ld bytes",
               size);

  free(file);
  logger_remove_test_files();
  return NULL;
}

static void logger_scenario_retry_after_failure(void) {
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_BAD_PATH) != 0,
               "the first init_logger should fail");
  logger_check(init_logger(TRACE, FILE_ONLY, TEST_LOG_PATH) == 0,
               "the second init_logger should succeed");
  logger_log_lines(0, 10);
  destroy_logger();
}

// A failed init must not poison a later, successful init.
void *logger_init_succeeds_after_failed_init_test() {
  logger_remove_test_files();
  logger_run_in_child(logger_scenario_retry_after_failure);

  long size = 0;
  char *buffer = logger_read_file(TEST_LOG_PATH, &size);
  logger_expect_lines(buffer, size, 0, 10);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");

  free(buffer);
  logger_remove_test_files();
  return NULL;
}

/* ------------------------------------------------------------------------ */
/* Test runner                                                              */
/* ------------------------------------------------------------------------ */

int logger_test() {
  logger_start_case(logger_file_has_no_zero_tail_test,
                    "logger_file_has_no_zero_tail_test");
  logger_start_case(logger_file_keeps_all_lines_across_remaps_test,
                    "logger_file_keeps_all_lines_across_remaps_test");
  logger_start_case(logger_file_appends_across_runs_test,
                    "logger_file_appends_across_runs_test");

  logger_start_case(logger_console_only_destroy_test,
                    "logger_console_only_destroy_test");
  logger_start_case(logger_console_and_file_destination_test,
                    "logger_console_and_file_destination_test");
  logger_start_case(logger_log_after_destroy_test,
                    "logger_log_after_destroy_test");

  logger_start_case(logger_level_filter_test, "logger_level_filter_test");
  logger_start_case(logger_errno_is_only_reported_by_log_errno_test,
                    "logger_errno_is_only_reported_by_log_errno_test");
  logger_start_case(logger_error_backtrace_test, "logger_error_backtrace_test");
  logger_start_case(logger_deep_backtrace_is_truncated_test,
                    "logger_deep_backtrace_is_truncated_test");
  logger_start_case(logger_error_v_formats_arguments_test,
                    "logger_error_v_formats_arguments_test");

  logger_start_case(logger_long_line_is_truncated_test,
                    "logger_long_line_is_truncated_test");
  logger_start_case(logger_line_length_boundaries_test,
                    "logger_line_length_boundaries_test");

  logger_start_case(logger_remap_failure_falls_back_to_console_test,
                    "logger_remap_failure_falls_back_to_console_test");

  logger_start_case(logger_init_open_failure_falls_back_to_console_test,
                    "logger_init_open_failure_falls_back_to_console_test");
  logger_start_case(logger_init_truncate_failure_leaves_clean_state_test,
                    "logger_init_truncate_failure_leaves_clean_state_test");
  logger_start_case(logger_init_succeeds_after_failed_init_test,
                    "logger_init_succeeds_after_failed_init_test");

  // Keep known-bug tests last: a failing assert aborts the whole run.
  logger_start_case(logger_remap_failure_does_not_lose_a_line_test,
                    "logger_remap_failure_does_not_lose_a_line_test");

  return logger_test_count;
}

static int logger_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test case %d: %s\n", logger_test_count++, name);

  func(NULL);

  return 0;
}
