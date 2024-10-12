#include "compression.h"
#include <stdio.h>
#include <zlib.h>
#include <errno.h>

int compress_gzip(char *buffer, size_t buffer_size, char *to_buffer) {
  // Create a temporary file to store the gzip output
  FILE *tmp_file = tmpfile();
  if (!tmp_file) {
    perror("Failed to create temporary file");
    return -1;
  }
  printf("tmp_file: %p\n", tmp_file);

  // Open the temporary file with zlib's gzopen
  gzFile gzfile = gzdopen(dup(fileno(tmp_file)), "wb");
  if (!gzfile) {
    perror("Failed to open gzip file");
    fclose(tmp_file);
    return -1;
  }
  printf("gzfile: %p\n", gzfile);

  // Write the buffer to the gzip file
  int bytes_written = gzwrite(gzfile, buffer, buffer_size);
  if (bytes_written <= 0 || bytes_written != buffer_size) {
    int err;
    const char *error_string = gzerror(gzfile, &err);
    fprintf(stderr, "Failed to write to gzip file: %s\n", error_string);
    gzclose(gzfile);
    fclose(tmp_file);
    return -1;
  }
  printf("bytes_written: %d\n", bytes_written);
  printf("%d\n", gzeof(gzfile));

  int close_result = gzclose(gzfile);
  if(close_result != Z_OK) {
    perror("Failed to close gzip file");
    fclose(tmp_file);
    return -1;
  }

  // Get the size of the compressed data
  fseek(tmp_file, 0, SEEK_END);
  size_t compressed_size = ftell(tmp_file);
  printf("compressed_size: %ld\n", compressed_size);
  rewind(tmp_file);
  if (compressed_size <= 0) {
    perror("Failed to get compressed file size");
    fclose(tmp_file);
    return -1;
  }

  // Read the compressed data into the buffer
  fread(to_buffer, 1, compressed_size, tmp_file);
  fclose(tmp_file);

  return compressed_size;
}

