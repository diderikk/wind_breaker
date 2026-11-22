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

// Mirrors LOG_BUFFER_SIZE in logger.c (it is not exposed in the header). If
// you change the buffer size there, change it here too: the remap tests below
// depend on it.
#define LOGGER_TEST_BUFFER_SIZE (1 * 1024 * 1024)
// Pads every line (~210 bytes with the header) so a few tens of thousands of
// lines cross many remaps (a remap happens roughly every 1 MiB).
#define LOGGER_TEST_PADDING                                                    \
  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" \
  "xxxxxxxxxx"
#define LOGGER_TEST_MANY_LINES 60000
// Enough lines to cross the first remap (~4900 lines per MiB) with some left
// over. Kept small because the console capture file shares the file size limit
// the remap-failure scenario sets.
#define LOGGER_TEST_REMAP_LINES 6000

static int logger_test_count = 0;
static char logger_test_path[64];
static char logger_test_console_path[64];
static char logger_test_stderr_path[64];
// A directory that does not exist, so fopen() on it fails.
#define LOGGER_TEST_BAD_PATH "/wb_logger_test_missing_dir/x.log"
static int logger_start_case(void *(*func)(void *), const char *name);

/* ------------------------------------------------------------------------ */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------ */

static void logger_check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "  FAILED: %s\n", message);
  }
  assert(condition);
}

static void logger_test_new_path(void) {
  snprintf(logger_test_path, sizeof(logger_test_path),
           "/tmp/wb_logger_test_%d.log", (int)getpid());
  unlink(logger_test_path);
}

// Where a scenario redirects its stdout, so the parent can inspect what was
// printed to the console.
static void logger_test_new_console_path(void) {
  snprintf(logger_test_console_path, sizeof(logger_test_console_path),
           "/tmp/wb_logger_test_console_%d.txt", (int)getpid());
  unlink(logger_test_console_path);
}

static void logger_test_new_stderr_path(void) {
  snprintf(logger_test_stderr_path, sizeof(logger_test_stderr_path),
           "/tmp/wb_logger_test_stderr_%d.txt", (int)getpid());
  unlink(logger_test_stderr_path);
}

// Returns 1 if `needle` is somewhere in the file at `path`.
static int logger_file_contains(const char *path, const char *needle) {
  FILE *f = fopen(path, "rb");
  if (f == NULL)
    return 0;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buffer = malloc(size > 0 ? size : 1);
  size_t read = fread(buffer, 1, size, f);
  fclose(f);
  int found = memmem(buffer, read, needle, strlen(needle)) != NULL;
  free(buffer);
  return found;
}

// Number of open file descriptors in this process (used to detect leaked
// FILE*s after a failed init).
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

// Returns the number of the first / last "LINE nnnnnn" marker in the buffer,
// or -1 if there is none.
static int logger_first_line_number(const char *buffer, long size) {
  for (long i = 0; i + 11 <= size; i++) {
    if (memcmp(buffer + i, "LINE ", 5) != 0)
      continue;
    int number = 0;
    for (int d = 0; d < 6; d++)
      number = number * 10 + (buffer[i + 5 + d] - '0');
    return number;
  }
  return -1;
}

static int logger_last_line_number(const char *buffer, long size) {
  int last = -1;
  for (long i = 0; i + 11 <= size; i++) {
    if (memcmp(buffer + i, "LINE ", 5) != 0)
      continue;
    int number = 0;
    for (int d = 0; d < 6; d++)
      number = number * 10 + (buffer[i + 5 + d] - '0');
    last = number;
  }
  return last;
}

static long logger_file_size(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f == NULL)
    return -1;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  return size;
}

static char *logger_read_path(const char *path, long *size) {
  FILE *f = fopen(path, "rb");
  logger_check(f != NULL, "file should exist");
  fseek(f, 0, SEEK_END);
  *size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buffer = malloc(*size > 0 ? *size : 1);
  logger_check(fread(buffer, 1, *size, f) == (size_t)*size,
               "log file should be readable");
  fclose(f);
  return buffer;
}

static char *logger_read_file(long *size) {
  return logger_read_path(logger_test_path, size);
}

static long logger_count_nul_bytes(const char *buffer, long size) {
  long count = 0;
  for (long i = 0; i < size; i++) {
    if (buffer[i] == '\0')
      count++;
  }
  return count;
}

static long logger_count_lines(const char *buffer, long size) {
  long count = 0;
  for (long i = 0; i + 5 <= size; i++) {
    if (memcmp(buffer + i, "LINE ", 5) == 0)
      count++;
  }
  return count;
}

// Looks for "LINE nnnnnn" markers and checks that they appear exactly once,
// in order, starting at `first` and ending at `last - 1`.
static void logger_expect_lines(const char *buffer, long size, int first,
                                int last) {
  int expected = first;
  for (long i = 0; i + 11 <= size; i++) {
    if (memcmp(buffer + i, "LINE ", 5) != 0)
      continue;
    int number = 0;
    for (int d = 0; d < 6; d++)
      number = number * 10 + (buffer[i + 5 + d] - '0');
    if (number != expected) {
      fprintf(stderr, "  expected LINE %06d but found LINE %06d at byte %ld\n",
              expected, number, i);
    }
    logger_check(number == expected,
                 "lines should appear once, in order, none lost or "
                 "overwritten");
    expected++;
  }
  if (expected != last) {
    fprintf(stderr, "  expected %d lines, found %d\n", last - first,
            expected - first);
  }
  logger_check(expected == last, "every logged line should be in the file");
}

static void logger_log_lines(int from, int to) {
  for (int i = from; i < to; i++) {
    log_info("LINE %06d " LOGGER_TEST_PADDING, i);
  }
}

// Logger state is global and the suite already initialised it in main(), so
// every scenario runs in a forked child. That keeps the rest of the suite
// untouched and lets us observe crashes as an exit status.
static int logger_run_in_child(void (*scenario)(void)) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  logger_check(pid >= 0, "fork should work");
  if (pid == 0) {
    if (freopen("/dev/null", "w", stdout) == NULL)
      _exit(2);
    destroy_logger(); // drop the console logger inherited from the parent
    scenario();
    _exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  return status;
}

static void logger_expect_clean_exit(int status) {
  if (WIFSIGNALED(status)) {
    fprintf(stderr, "  child was killed by signal %d\n", WTERMSIG(status));
  }
  logger_check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
               "scenario should exit cleanly (no crash)");
}

/* ------------------------------------------------------------------------ */
/* Scenarios (run in a child process)                                       */
/* ------------------------------------------------------------------------ */

static void logger_scenario_few_lines(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_lines(0, 10);
  destroy_logger();
}

static void logger_scenario_many_lines(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_lines(0, LOGGER_TEST_MANY_LINES);
  destroy_logger();
}

static void logger_scenario_first_run(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_lines(0, 100);
  destroy_logger();
}

static void logger_scenario_second_run(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_lines(100, 200);
  destroy_logger();
}

static void logger_scenario_console_only(void) {
  if (init_logger(TRACE, CONSOLE_ONLY, NULL) != 0)
    _exit(3);
  logger_log_lines(0, 5);
  destroy_logger();
}

// One message far longer than the per-line temp buffer, followed by normal
// lines.
static void logger_scenario_long_line(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  char long_message[20000];
  memset(long_message, 'y', sizeof(long_message) - 1);
  long_message[sizeof(long_message) - 1] = '\0';
  log_info("%s", long_message);
  logger_log_lines(0, 3);
  destroy_logger();
}

#define LOGGER_TEST_SWEEP_FIRST 3800
#define LOGGER_TEST_SWEEP_LAST 4300

// Messages of every length around the per-line buffer size (4 KiB), so the
// "exactly fits" and "one byte too long" cases are both hit whatever the
// header length is.
static void logger_scenario_length_sweep(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  char message[LOGGER_TEST_SWEEP_LAST + 1];
  for (int length = LOGGER_TEST_SWEEP_FIRST; length <= LOGGER_TEST_SWEEP_LAST;
       length++) {
    memset(message, 'z', length);
    message[length] = '\0';
    log_info("%s", message);
  }
  destroy_logger();
}

// Sends the child's console output to logger_test_console_path and silences
// its stderr (init_logger reports failures there).
static void logger_scenario_capture_console(void) {
  if (freopen("/dev/null", "w", stderr) == NULL)
    _exit(2);
  if (freopen(logger_test_console_path, "w", stdout) == NULL)
    _exit(2);
}

// Like the above, but keeps stderr so a test can check which error paths ran.
static void logger_scenario_capture_console_and_stderr(void) {
  if (freopen(logger_test_stderr_path, "w", stderr) == NULL)
    _exit(2);
  // Scenarios end with _exit(), which does not flush stdio buffers.
  setvbuf(stderr, NULL, _IONBF, 0);
  if (freopen(logger_test_console_path, "w", stdout) == NULL)
    _exit(2);
}

// Forces the first remap to fail: the file size limit allows the initial 1 MiB
// mapping but not the next one (ftruncate fails with EFBIG).
static void logger_scenario_remap_failure(void) {
  logger_scenario_capture_console_and_stderr();
  signal(SIGXFSZ, SIG_IGN);
  struct rlimit limit = {.rlim_cur = LOGGER_TEST_BUFFER_SIZE * 3 / 2,
                         .rlim_max = LOGGER_TEST_BUFFER_SIZE * 3 / 2};
  if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
    _exit(4);
  alarm(60); // a hang (e.g. a deadlock) turns into SIGALRM instead of a stall

  if (init_logger(INFO, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_lines(0, LOGGER_TEST_REMAP_LINES);
  destroy_logger();
  fflush(stdout);
}

// Logging after destroy_logger() must not touch the unmapped buffer: it goes to
// the console only, and never into the (already closed) file.
static void logger_scenario_log_after_destroy(void) {
  logger_scenario_capture_console();
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_lines(0, 3);
  destroy_logger();
  log_info("AFTER DESTROY");
  log_error("ERROR AFTER DESTROY");
  fflush(stdout);
}

// Only messages at or above the configured level may be written.
static void logger_scenario_level_filter(void) {
  if (init_logger(WARN, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  log_trace("TRACE MSG");
  log_debug("DEBUG MSG");
  log_info("INFO MSG");
  log_warn("WARN MSG");
  log_error("ERROR MSG");
  destroy_logger();
}

// CONSOLE_FILE writes every line to both places.
static void logger_scenario_console_and_file(void) {
  logger_scenario_capture_console();
  if (init_logger(TRACE, CONSOLE_FILE, logger_test_path) != 0)
    _exit(3);
  logger_log_lines(0, 5);
  destroy_logger();
  fflush(stdout);
}

// errno is only reported through log_errno(); a stale errno must never leak
// into a plain log_error().
static void logger_scenario_errno(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  errno = ENOENT;
  log_error("PLAIN ERROR MSG");
  errno = ENOENT;
  log_errno("ERRNO MSG");
  errno = 0;
  log_errno("NO ERRNO MSG");
  logger_log_lines(0, 3);
  destroy_logger();
}

static void logger_scenario_error_backtrace(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  errno = ENOENT; // must not be reported by log_error_backtrace()
  log_error_backtrace("BACKTRACE MSG");
  logger_log_lines(0, 3);
  destroy_logger();
}

static void logger_log_error_via_va_list(const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_error_v(format, args);
  va_end(args);
}

static void logger_scenario_error_v(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_error_via_va_list("VA_LIST MSG %d %s", 42, "forty-two");
  logger_log_lines(0, 3);
  destroy_logger();
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
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(3);
  logger_log_from_deep_stack(200);
  logger_log_lines(0, 3);
  destroy_logger();
}

// init_logger must fail when the log file cannot be opened, and a failed
// init must leave the logger usable: console-only, at the previous level.
static void logger_scenario_open_failure(void) {
  logger_scenario_capture_console();
  // ERROR + FILE_ONLY must NOT be applied if init fails: otherwise the
  // INFO line below would be filtered out or sent nowhere.
  if (init_logger(ERROR, FILE_ONLY, LOGGER_TEST_BAD_PATH) == 0)
    _exit(3);
  log_info("AFTER FAILED INIT");
  fflush(stdout);
  destroy_logger(); // must be safe on a logger that never finished init
  log_info("AFTER DESTROY");
  fflush(stdout);
}

// ftruncate() of the new log file fails (file size limit), which is a failure
// after the file was already opened.
static void logger_scenario_truncate_failure(void) {
  logger_scenario_capture_console();
  signal(SIGXFSZ, SIG_IGN);
  struct rlimit limit = {.rlim_cur = 1024, .rlim_max = 1024};
  if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
    _exit(4);
  alarm(60);

  int fds_before = logger_count_open_fds();
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) == 0)
    _exit(3);
  // The FILE* opened by init_logger must be closed again on failure.
  if (logger_count_open_fds() != fds_before)
    _exit(5);
  log_info("AFTER FAILED INIT");
  log_error("ERROR AFTER FAILED INIT");
  fflush(stdout);
  destroy_logger();
}

// A failed init must not poison a later, successful init.
static void logger_scenario_retry_after_failure(void) {
  logger_scenario_capture_console();
  if (init_logger(TRACE, FILE_ONLY, LOGGER_TEST_BAD_PATH) == 0)
    _exit(3);
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) != 0)
    _exit(4);
  logger_log_lines(0, 10);
  destroy_logger();
}

/* ------------------------------------------------------------------------ */
/* Tests                                                                    */
/* ------------------------------------------------------------------------ */

// Review issue: destroy_logger never shrank the file, so every run left the
// unused part of the mapping behind as zeros.
void *logger_file_has_no_zero_tail_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_few_lines));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(size > 0, "file should contain the logged lines");
  logger_check(size < LOGGER_TEST_BUFFER_SIZE / 16,
               "file should be trimmed to the written bytes, not left at the "
               "full mapping size");
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");
  logger_check(buffer[size - 1] == '\n',
               "last byte of the file should be the end of the last log line");

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// Review issue 1: file_offset must follow the mapping after every remap,
// otherwise a later chunk is mapped on top of earlier logs and destroy_logger
// truncates away everything after the first chunk.
void *logger_file_keeps_all_lines_across_remaps_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_many_lines));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(size > 3 * LOGGER_TEST_BUFFER_SIZE,
               "test should have written enough to trigger several remaps");
  logger_expect_lines(buffer, size, 0, LOGGER_TEST_MANY_LINES);

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// Original review issue: remapping used to leave ~1 MiB of zeros between
// chunks. After a run that remapped several times the file must be gap-free.
void *logger_file_has_no_zero_gaps_across_remaps_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_many_lines));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(size > 3 * LOGGER_TEST_BUFFER_SIZE,
               "test should have written enough to trigger several remaps");
  long nul_bytes = logger_count_nul_bytes(buffer, size);
  if (nul_bytes > 0) {
    fprintf(stderr, "  found %ld NUL bytes in a %ld byte file\n", nul_bytes,
            size);
  }
  logger_check(nul_bytes == 0, "log file should not contain any zero bytes");

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// A second run appends to the file. It must not carry over a zero tail from
// the first run, and the first run's lines must still be there.
void *logger_file_appends_across_runs_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_first_run));
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_second_run));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_expect_lines(buffer, size, 0, 200);
  logger_check(buffer[size - 1] == '\n',
               "file should end with the last line of the second run");

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// Review issue 2: with no log file, destroy_logger used to call
// fileno(NULL).
void *logger_console_only_destroy_test() {
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_console_only));
  return NULL;
}

// A message longer than the temp buffer must be cut short, still end in a
// newline, not leak into the next line, and not corrupt neighbouring memory.
void *logger_long_line_is_truncated_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_long_line));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "truncating a long line must not leave NUL bytes in the file");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");
  logger_check(size < 20000, "the long message should have been truncated");
  logger_check(memmem(buffer, size, "...\n", 4) != NULL,
               "a truncated line should end with ...\\n");
  // The lines after the long one must be intact and start on their own line.
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// Whatever the message length is (just under, exactly at, or over the temp
// buffer size), every log call must produce exactly one newline-terminated
// line and no NUL bytes.
void *logger_line_length_boundaries_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_length_sweep));

  long size = 0;
  char *buffer = logger_read_file(&size);
  long nul_bytes = logger_count_nul_bytes(buffer, size);
  long newlines = 0;
  for (long i = 0; i < size; i++) {
    if (buffer[i] == '\n')
      newlines++;
  }
  // one line per message, plus the "Logger initialized" line
  long expected_lines =
      LOGGER_TEST_SWEEP_LAST - LOGGER_TEST_SWEEP_FIRST + 1 + 1;
  if (nul_bytes != 0 || newlines != expected_lines) {
    fprintf(stderr, "  %ld NUL bytes, %ld newlines (expected 0 and %ld)\n",
            nul_bytes, newlines, expected_lines);
  }
  logger_check(nul_bytes == 0, "no line length may produce NUL bytes");
  logger_check(newlines == expected_lines,
               "every message should end with exactly one newline");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// A failed remap must not crash, hang, or corrupt what was already written.
// The logger is expected to fall back to console-only logging.
void *logger_remap_failure_falls_back_to_console_test() {
  logger_test_new_path();
  logger_test_new_console_path();
  logger_test_new_stderr_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_remap_failure));

  // Guard against a vacuous pass: the failure path must really have run (the
  // file size limit in the scenario depends on LOGGER_TEST_BUFFER_SIZE).
  logger_check(logger_file_contains(logger_test_stderr_path,
                                    "Failed to remap log buffer"),
               "the scenario should have triggered a remap failure");

  // Everything logged before the failure must still be in the file, and the
  // file must not be left with a zero tail.
  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(size > 0, "lines logged before the failure should be kept");
  logger_check(size <= LOGGER_TEST_BUFFER_SIZE * 3 / 2,
               "the file should not grow past the size limit");
  logger_check(buffer[size - 1] == '\n',
               "file should end at the last line written before the failure");
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");
  int lines_in_file = (int)logger_count_lines(buffer, size);
  logger_check(lines_in_file > 1000 && lines_in_file < LOGGER_TEST_REMAP_LINES,
               "the failure should happen part way through the run");
  logger_expect_lines(buffer, size, 0, lines_in_file);

  // After the failure the logger keeps working, on the console.
  long console_size = 0;
  char *console = logger_read_path(logger_test_console_path, &console_size);
  logger_check(logger_last_line_number(console, console_size) ==
                   LOGGER_TEST_REMAP_LINES - 1,
               "lines logged after the failure should reach the console");

  free(console);
  free(buffer);
  unlink(logger_test_path);
  unlink(logger_test_console_path);
  unlink(logger_test_stderr_path);
  return NULL;
}

// The line that triggers a failed remap must not vanish: it is neither in the
// file (the mapping is gone) nor, for FILE_ONLY, on the console. Every line
// should be in the file or on the console.
// KNOWN BUG until log_message() writes the current entry to the console when
// the fallback happens.
void *logger_remap_failure_does_not_lose_a_line_test() {
  logger_test_new_path();
  logger_test_new_console_path();
  logger_test_new_stderr_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_remap_failure));

  long size = 0, console_size = 0;
  char *buffer = logger_read_file(&size);
  char *console = logger_read_path(logger_test_console_path, &console_size);
  int last_in_file = logger_last_line_number(buffer, size);
  int first_on_console = logger_first_line_number(console, console_size);
  if (first_on_console != last_in_file + 1) {
    fprintf(stderr, "  last line in file: %d, first line on console: %d\n",
            last_in_file, first_on_console);
  }
  logger_check(first_on_console == last_in_file + 1,
               "no line should be lost between the file and the console");

  free(console);
  free(buffer);
  unlink(logger_test_path);
  unlink(logger_test_console_path);
  unlink(logger_test_stderr_path);
  return NULL;
}

// Logging after destroy_logger() falls back to the console and must not touch
// the unmapped buffer or the closed file.
void *logger_log_after_destroy_test() {
  logger_test_new_path();
  logger_test_new_console_path();
  logger_expect_clean_exit(
      logger_run_in_child(logger_scenario_log_after_destroy));

  logger_check(logger_file_contains(logger_test_console_path, "AFTER DESTROY"),
               "logging after destroy should reach the console");
  logger_check(
      logger_file_contains(logger_test_console_path, "ERROR AFTER DESTROY"),
      "errors after destroy should reach the console");
  logger_check(!logger_file_contains(logger_test_path, "AFTER DESTROY"),
               "logging after destroy must not write to the log file");
  logger_check(logger_file_contains(logger_test_path, "LINE 000002"),
               "lines logged before destroy should be in the file");

  unlink(logger_test_path);
  unlink(logger_test_console_path);
  return NULL;
}

void *logger_level_filter_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_level_filter));

  logger_check(!logger_file_contains(logger_test_path, "TRACE MSG"),
               "TRACE should be filtered out at level WARN");
  logger_check(!logger_file_contains(logger_test_path, "DEBUG MSG"),
               "DEBUG should be filtered out at level WARN");
  logger_check(!logger_file_contains(logger_test_path, "INFO MSG"),
               "INFO should be filtered out at level WARN");
  logger_check(logger_file_contains(logger_test_path, "WARN MSG"),
               "WARN should be logged at level WARN");
  logger_check(logger_file_contains(logger_test_path, "ERROR MSG"),
               "ERROR should be logged at level WARN");

  unlink(logger_test_path);
  return NULL;
}

void *logger_console_and_file_destination_test() {
  logger_test_new_path();
  logger_test_new_console_path();
  logger_expect_clean_exit(
      logger_run_in_child(logger_scenario_console_and_file));

  long size = 0, console_size = 0;
  char *buffer = logger_read_file(&size);
  char *console = logger_read_path(logger_test_console_path, &console_size);
  logger_expect_lines(buffer, size, 0, 5);
  logger_expect_lines(console, console_size, 0, 5);

  free(console);
  free(buffer);
  unlink(logger_test_path);
  unlink(logger_test_console_path);
  return NULL;
}

// log_errno() reports errno (and a backtrace); log_error() must not pick up a
// stale errno.
void *logger_errno_is_only_reported_by_log_errno_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_errno));

  const char *enoent = strerror(ENOENT);
  long size = 0;
  char *buffer = logger_read_file(&size);

  logger_check(logger_file_contains(logger_test_path, "PLAIN ERROR MSG"),
               "log_error should be logged");
  logger_check(logger_file_contains(logger_test_path, "ERRNO MSG"),
               "log_errno should be logged");
  logger_check(memmem(buffer, size, enoent, strlen(enoent)) != NULL,
               "log_errno should include the errno text");
  logger_check(logger_file_contains(logger_test_path, "Backtrace:"),
               "log_errno should include a backtrace");

  // Exactly one ENOENT line: the one from log_errno(). log_error() with a
  // stale errno and log_errno() with errno == 0 must not produce one.
  int occurrences = 0;
  for (long i = 0; i + (long)strlen(enoent) <= size; i++) {
    if (memcmp(buffer + i, enoent, strlen(enoent)) == 0)
      occurrences++;
  }
  logger_check(occurrences == 1,
               "only log_errno with errno set should report the errno text");
  logger_expect_lines(buffer, size, 0, 3);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

void *logger_error_backtrace_test() {
  logger_test_new_path();
  logger_expect_clean_exit(
      logger_run_in_child(logger_scenario_error_backtrace));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(logger_file_contains(logger_test_path, "BACKTRACE MSG"),
               "message should be logged");
  logger_check(logger_file_contains(logger_test_path, "Backtrace:"),
               "log_error_backtrace should include a backtrace");
  logger_check(!logger_file_contains(logger_test_path, strerror(ENOENT)),
               "log_error_backtrace should not report errno");
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// A backtrace that does not fit in the per-line buffer is cut with "...",
// and the next log line must still start cleanly.
void *logger_deep_backtrace_is_truncated_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_deep_backtrace));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(logger_file_contains(logger_test_path, "DEEP BACKTRACE MSG"),
               "message should be logged");
  logger_check(memmem(buffer, size, "...\n", 4) != NULL,
               "an oversized backtrace should end with ...\\n");
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "file should not contain any zero bytes");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

void *logger_error_v_formats_arguments_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_error_v));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(
      logger_file_contains(logger_test_path, "VA_LIST MSG 42 forty-two"),
      "log_error_v should format the va_list arguments");
  logger_expect_lines(buffer, size, 0, 3);

  free(buffer);
  unlink(logger_test_path);
  return NULL;
}

// init_logger reports failure (non-zero) when the log file cannot be opened,
// and the logger keeps working as a console logger afterwards: the requested
// level/destination are not applied, nothing crashes, destroy_logger is safe.
void *logger_init_open_failure_falls_back_to_console_test() {
  logger_test_new_console_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_open_failure));

  logger_check(
      logger_file_contains(logger_test_console_path, "AFTER FAILED INIT"),
      "after a failed init the logger should still log to the console, at the "
      "previous level");
  logger_check(logger_file_contains(logger_test_console_path, "AFTER DESTROY"),
               "logging after destroy_logger should not crash");
  logger_check(
      !logger_file_contains(logger_test_console_path, "Logger initialized"),
      "a failed init must not report success");

  unlink(logger_test_console_path);
  return NULL;
}

// Failure after the file was opened (ftruncate fails): init reports failure,
// does not crash on later log calls, and leaves the file as it found it.
void *logger_init_truncate_failure_leaves_clean_state_test() {
  logger_test_new_path();
  logger_test_new_console_path();
  logger_expect_clean_exit(
      logger_run_in_child(logger_scenario_truncate_failure));

  logger_check(
      logger_file_contains(logger_test_console_path, "ERROR AFTER FAILED INIT"),
      "errors logged after a failed init should reach the console");
  long size = logger_file_size(logger_test_path);
  if (size != 0) {
    fprintf(stderr, "  log file is %ld bytes, expected 0\n", size);
  }
  logger_check(size == 0,
               "a failed init must not leave the file extended with zeros or "
               "write log lines into it");

  unlink(logger_test_path);
  unlink(logger_test_console_path);
  return NULL;
}

// A failed init followed by a good one must produce a normal log file.
void *logger_init_succeeds_after_failed_init_test() {
  logger_test_new_path();
  logger_test_new_console_path();
  logger_expect_clean_exit(
      logger_run_in_child(logger_scenario_retry_after_failure));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_expect_lines(buffer, size, 0, 10);
  logger_check(logger_count_nul_bytes(buffer, size) == 0,
               "log file should not contain any zero bytes");
  logger_check(buffer[size - 1] == '\n', "file should end with a newline");

  free(buffer);
  unlink(logger_test_path);
  unlink(logger_test_console_path);
  return NULL;
}

int logger_test() {
  logger_start_case(logger_file_has_no_zero_tail_test,
                    "logger_file_has_no_zero_tail_test");
  logger_start_case(logger_file_keeps_all_lines_across_remaps_test,
                    "logger_file_keeps_all_lines_across_remaps_test");
  logger_start_case(logger_file_has_no_zero_gaps_across_remaps_test,
                    "logger_file_has_no_zero_gaps_across_remaps_test");
  logger_start_case(logger_file_appends_across_runs_test,
                    "logger_file_appends_across_runs_test");
  logger_start_case(logger_console_only_destroy_test,
                    "logger_console_only_destroy_test");
  logger_start_case(logger_remap_failure_falls_back_to_console_test,
                    "logger_remap_failure_falls_back_to_console_test");
  logger_start_case(logger_long_line_is_truncated_test,
                    "logger_long_line_is_truncated_test");
  logger_start_case(logger_line_length_boundaries_test,
                    "logger_line_length_boundaries_test");

  logger_start_case(logger_init_open_failure_falls_back_to_console_test,
                    "logger_init_open_failure_falls_back_to_console_test");
  logger_start_case(logger_init_truncate_failure_leaves_clean_state_test,
                    "logger_init_truncate_failure_leaves_clean_state_test");
  logger_start_case(logger_init_succeeds_after_failed_init_test,
                    "logger_init_succeeds_after_failed_init_test");

  logger_start_case(logger_log_after_destroy_test,
                    "logger_log_after_destroy_test");
  logger_start_case(logger_level_filter_test, "logger_level_filter_test");
  logger_start_case(logger_console_and_file_destination_test,
                    "logger_console_and_file_destination_test");
  logger_start_case(logger_errno_is_only_reported_by_log_errno_test,
                    "logger_errno_is_only_reported_by_log_errno_test");
  logger_start_case(logger_error_backtrace_test, "logger_error_backtrace_test");
  logger_start_case(logger_deep_backtrace_is_truncated_test,
                    "logger_deep_backtrace_is_truncated_test");
  logger_start_case(logger_error_v_formats_arguments_test,
                    "logger_error_v_formats_arguments_test");

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
