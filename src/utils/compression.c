#include "compression.h"

int compress_gzip(char *buffer, size_t buffer_size, char *to_buffer) {
  // Create a temporary file to store the gzip output
  FILE *tmp_file = tmpfile();
  if (!tmp_file) {
    perror("Failed to create temporary file");
    return -1;
  }

  // Open the temporary file with zlib's gzopen
  gzFile gzfile = gzdopen(fileno(tmp_file), "wb");
  if (!gzfile) {
    perror("Failed to open gzip file");
    fclose(tmp_file);
    return -1;
  }

  // Write the buffer to the gzip file
  int bytes_written = gzwrite(gzfile, buffer, buffer_size);
  if (bytes_written <= 0) {
    int err;
    const char *error_string = gzerror(gzfile, &err);
    fprintf(stderr, "Failed to write to gzip file: %s\n", error_string);
    gzclose(gzfile);
    fclose(tmp_file);
    return -1;
  }

  gzclose(gzfile);

  // Get the size of the compressed data
  fseek(tmp_file, 0, SEEK_END);
  size_t compressed_size = ftell(tmp_file);
  rewind(tmp_file);

  // Allocate memory for the compressed data
  to_buffer = malloc(compressed_size);
  if (!*to_buffer) {
    perror("Failed to allocate memory for compressed buffer");
    fclose(tmp_file);
    return -1;
  }

  // Read the compressed data into the buffer
  fread(to_buffer, 1, compressed_size, tmp_file);
  fclose(tmp_file);

  return 0;
}
