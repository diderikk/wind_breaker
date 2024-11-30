#include "compression.h"
#include "assert2.h"
#include "logger.h"
#include <errno.h>

int compress_gzip(char *buffer, size_t buffer_size, char *to_buffer) {
  // Create a temporary file to store the gzip output
  FILE *tmp_file = tmpfile();

  if (!tmp_file) {
    log_error("Failed to create temporary file");
    return -1;
  }
  log_trace("tmp_file: %p", tmp_file);

  // Open the temporary file with zlib's gzopen
  gzFile gzfile = gzdopen(dup(fileno(tmp_file)), "wb");
  if (!gzfile) {
    log_error("Failed to open gzip file");
    fclose(tmp_file);
    return -1;
  }
  log_trace("gzfile: %p", gzfile);

  // Write the buffer to the gzip file
  int bytes_written = gzwrite(gzfile, buffer, buffer_size);
  if (bytes_written <= 0 || bytes_written != buffer_size) {
    int err;
    const char *error_string = gzerror(gzfile, &err);
    log_error("Failed to write to gzip file: %s", error_string);
    gzclose(gzfile);
    fclose(tmp_file);
    return -1;
  }
  log_trace("bytes_written: %d", bytes_written);

  int close_result = gzclose(gzfile);
  if (close_result != Z_OK) {
    log_error("Failed to close gzip file");
    fclose(tmp_file);
    return -1;
  }

  // Get the size of the compressed data
  fseek(tmp_file, 0, SEEK_END);
  size_t compressed_size = ftell(tmp_file);
  log_trace("compressed_size: %ld", compressed_size);
  rewind(tmp_file);
  if (compressed_size <= 0) {
    log_error("Failed to get compressed file size");
    fclose(tmp_file);
    return -1;
  }

  // Read the compressed data into the buffer
  fread(to_buffer, 1, compressed_size, tmp_file);
  fclose(tmp_file);

  return compressed_size;
}

int compress_deflate(char *buffer, size_t buffer_size, char *to_buffer) {
  uLong compressed_len = compressBound(buffer_size);

  z_stream defstream;
  defstream.zalloc = Z_NULL;
  defstream.zfree = Z_NULL;
  defstream.opaque = Z_NULL;

  defstream.avail_in = (uInt)buffer_size;
  defstream.next_in = (Bytef *)buffer;
  defstream.avail_out = (uInt)compressed_len;
  defstream.next_out = (Bytef *)to_buffer;

  deflateInit(&defstream, Z_BEST_COMPRESSION);
  deflate(&defstream, Z_FINISH);
  deflateEnd(&defstream);

  log_trace("Original size: %lu, Compressed size: %lu", buffer_size,
            defstream.total_out);

  return defstream.total_out;
}
