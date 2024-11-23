#include "file.h"
#include "logger.h"
#include <stdio.h>
#include <unistd.h>

#define STATIC_PATH "static/"

int find_static_file(char *uri) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, uri);

  if (access(full_path, F_OK) == 0) {
    log_trace("Found static file: %s", full_path);
    return 1;
  } else {
    log_error("Static file not found: %s", full_path);
    return 0;
  }
}

int read_static_file(char *file_path, char *buffer, size_t buffer_size) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_path);

  FILE *file = fopen(full_path, "rb");
  if (file == NULL) {
    return -1;
  }

  size_t read_size = fread(buffer, 1, buffer_size, file);
  fclose(file);

  log_trace("read_size: %ld", read_size);

  return read_size;
}

int write_static_file(char *file_name, char *buffer, size_t buffer_size) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_name);

  FILE *file = fopen(full_path, "wb");
  if (file == NULL) {
    log_error("Failed to open file: %s", full_path);
    return -1;
  }

  size_t write_size = fwrite(buffer, 1, buffer_size, file);
  fclose(file);
  log_trace("Wrote %ld bytes to file: %s", write_size, full_path);

  return write_size;
}
