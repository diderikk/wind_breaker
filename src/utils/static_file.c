#include "static_file.h"
#include "assert2.h"
#include "logger.h"
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define STATIC_PATH "static/"

int find_static_file(char *uri) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, uri);

  assert_log(access(full_path, F_OK) == 0, "Static file not found: %s",
             full_path);
  log_trace("Found static file: %s", full_path);
  return 1;
}

int read_static_file(char *file_path, char *buffer, size_t buffer_size) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_path);

  FILE *file = fopen(full_path, "rb");
  assert_log(file != NULL, "Failed to open file: %s", full_path);

  size_t read_size = fread(buffer, 1, buffer_size, file);
  assert_log(read_size > 0, "Failed to read file: %s", full_path);
  fclose(file);

  log_trace("Read %ld bytes from file: %s", read_size, full_path);
  return read_size;
}

int write_static_file(char *file_name, char *buffer, size_t buffer_size) {
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

int get_last_modified(char *file_path) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_path);

  struct stat file_stat;
  assert_log(stat(full_path, &file_stat) == 0, "Failed to get file stat: %s",
             full_path);

  return file_stat.st_mtime;
}
