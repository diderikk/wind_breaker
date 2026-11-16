#include "../../../src/utils/assert2.h"
#include "../../../src/utils/logger.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Mirrors LOG_BUFFER_SIZE in logger.c (it is not exposed in the header).
#define LOGGER_TEST_BUFFER_SIZE (4 * 1024 * 1024)
// Pads every line so a few tens of thousands of lines cross several remaps
// (a remap happens roughly every 3 MiB).
#define LOGGER_TEST_PADDING                                                    \
  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" \
  "xxxxxxxxxx"
#define LOGGER_TEST_MANY_LINES 60000

static int logger_test_count = 0;
static char logger_test_path[64];
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

static char *logger_read_file(long *size) {
  FILE *f = fopen(logger_test_path, "rb");
  logger_check(f != NULL, "log file should exist");
  fseek(f, 0, SEEK_END);
  *size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buffer = malloc(*size > 0 ? *size : 1);
  logger_check(fread(buffer, 1, *size, f) == (size_t)*size,
               "log file should be readable");
  fclose(f);
  return buffer;
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
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) == NULL)
    _exit(3);
  logger_log_lines(0, 10);
  destroy_logger();
}

static void logger_scenario_many_lines(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) == NULL)
    _exit(3);
  logger_log_lines(0, LOGGER_TEST_MANY_LINES);
  destroy_logger();
}

static void logger_scenario_first_run(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) == NULL)
    _exit(3);
  logger_log_lines(0, 100);
  destroy_logger();
}

static void logger_scenario_second_run(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) == NULL)
    _exit(3);
  logger_log_lines(100, 200);
  destroy_logger();
}

static void logger_scenario_console_only(void) {
  if (init_logger(TRACE, CONSOLE_ONLY, NULL) != NULL)
    _exit(3);
  logger_log_lines(0, 5);
  destroy_logger();
}

// One message far longer than the per-line temp buffer, followed by normal
// lines.
static void logger_scenario_long_line(void) {
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) == NULL)
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
  if (init_logger(TRACE, FILE_ONLY, logger_test_path) == NULL)
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

// Forces the first remap to fail: the file size limit allows the initial
// 4 MiB mapping but not the next one (ftruncate fails with EFBIG).
static void logger_scenario_remap_failure(void) {
  signal(SIGXFSZ, SIG_IGN);
  struct rlimit limit = {.rlim_cur = 5 * 1024 * 1024,
                         .rlim_max = 5 * 1024 * 1024};
  if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
    _exit(4);
  alarm(60); // a hang (e.g. a deadlock) turns into SIGALRM instead of a stall

  if (init_logger(INFO, FILE_ONLY, logger_test_path) == NULL)
    _exit(3);
  // Enough lines to cross the first remap (~3 MiB), plus some after it.
  logger_log_lines(0, 25000);
  destroy_logger();
}

/* ------------------------------------------------------------------------ */
/* Tests                                                                    */
/* ------------------------------------------------------------------------ */

// Review issue: destroy_logger never shrank the file, so every run left the
// unused part of the 4 MiB mapping behind as zeros.
void *logger_file_has_no_zero_tail_test() {
  logger_test_new_path();
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_few_lines));

  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(size > 0, "file should contain the logged lines");
  logger_check(size < LOGGER_TEST_BUFFER_SIZE,
               "file should be trimmed to the written bytes, not left at the "
               "full 4 MiB mapping");
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
  logger_check(size > 3 * 1024 * 1024,
               "test should have written enough to trigger a remap");
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
  logger_check(size > 3 * 1024 * 1024,
               "test should have written enough to trigger a remap");
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
  logger_expect_clean_exit(logger_run_in_child(logger_scenario_remap_failure));

  // Everything logged before the failure must still be in the file, and the
  // file must not be left with a zero tail.
  long size = 0;
  char *buffer = logger_read_file(&size);
  logger_check(size > 0, "lines logged before the failure should be kept");
  logger_check(buffer[size - 1] == '\n',
               "file should end at the last line written before the failure");
  logger_expect_lines(buffer, size, 0, (int)logger_count_lines(buffer, size));

  free(buffer);
  unlink(logger_test_path);
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

  return logger_test_count;
}

static int logger_start_case(void *(*func)(void *), const char *name) {
  printf("Starting test case %d: %s\n", logger_test_count++, name);

  func(NULL);

  return 0;
}
