#include "file.h"
#include <stdio.h>
#include <unistd.h>

#define STATIC_PATH "static/"

int find_static_file(char *uri) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, uri);

  printf("Checking file: %s\n", full_path);

  if (access(full_path, F_OK) == 0) {
    return 1;
  }

  return 0;
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

  return read_size;
}

int write_static_file(char *file_name, char *buffer, size_t buffer_size) {
  char full_path[512 + sizeof(STATIC_PATH)];
  snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, file_name);

  FILE *file = fopen(full_path, "wb");
  if (file == NULL) {
    printf("Failed to open file: %s\n", full_path);
    return -1;
  }

  size_t write_size = fwrite(buffer, 1, buffer_size, file);
  fclose(file);
  printf("Wrote %ld bytes to file: %s\n", write_size, full_path);

  return write_size;
}
