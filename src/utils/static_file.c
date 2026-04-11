#include "static_file.h"
#include "assert2.h"
#include "logger.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define STATIC_PATH "static/"

int is_safe_path(const char *input) {
  // Reject absolute paths
  if (input[0] == '/' || strstr(input, "//"))
    return 0;
  // Reject directory traversal
  if (strstr(input, ".."))
    return 0;
  // Optionally, reject backslashes (for Windows)
  if (strchr(input, '\\'))
    return 0;
  return 1;
}

int find_static_file(const char *uri) {
  if (!is_safe_path(uri)) {
    return 0;
  }

  char full_path[PATH_MAX];
  int needed = snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, uri);
  if (needed < 0 || needed >= sizeof(full_path)) {
    return 0;
  }

  char static_root[PATH_MAX];
  if (!realpath(STATIC_PATH, static_root)) {
    return 0;
  }

  char resolved_path[PATH_MAX];
  if (!realpath(full_path, resolved_path)) {
    return 0;
  }

  size_t root_len = strlen(static_root);
  if (strncmp(resolved_path, static_root, root_len) != 0 ||
      (resolved_path[root_len] != '/' && resolved_path[root_len] != '\0')) {
    return 0;
  }

  struct stat sb;
  if (stat(resolved_path, &sb) != 0 || !S_ISREG(sb.st_mode) ||
      access(resolved_path, R_OK) != 0) {
    // log_warn("File not regular or unreadable");
    return 0;
  }
  log_trace("Found static file: %s", full_path);
  return 1;
}

int read_static_file(const char *file_path, buffer* buffer) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_path);

  FILE *file = fopen(full_path, "rb");
  assert_log(file != NULL, "Failed to open file: %s", full_path);

  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);

  if(buffer->capacity < size) {
    assert(ENSURE_CAPACITY(buffer, size) > 0);
  }

  size_t read_size = fread(buffer->data, 1, size, file);

  buffer->count = read_size;
  assert_log(read_size > 0, "Failed to read file: %s", full_path);
  fclose(file);

  log_trace("Read %ld bytes from file: %s", read_size, full_path);
  return read_size;
}

int write_static_file(const char *file_name, char *buffer,
                      unsigned int buffer_size) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_name);

  FILE *file = fopen(full_path, "wb");
  assert_log(file != NULL, "Failed to open file: %s", full_path);

  size_t write_size = fwrite(buffer, 1, buffer_size, file);
  fclose(file);

  assert_log(write_size > 0, "Failed to write to file: %s", full_path);
  log_trace("Wrote %ld bytes to file: %s", write_size, full_path);
  return write_size;
}

int get_last_modified(const char *file_path) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_path);

  struct stat file_stat;
  assert_log(stat(full_path, &file_stat) == 0, "Failed to get file stat: %s",
             full_path);

  return file_stat.st_mtime;
}
