#include "compression.h"
#include "assert2.h"
#include "logger.h"
#include <errno.h>

int compress_gzip(const char *buffer, size_t buffer_size, char *to_buffer) {
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

int decompress_gzip(const char *buffer, size_t buffer_size, char *to_buffer,
                    size_t to_buffer_size) {
  FILE *tmp_file = tmpfile();
  if (!tmp_file) {
    log_error("Failed to create temporary file");
    return -1;
  }
  log_trace("tmp_file: %p", tmp_file);

  if (fwrite(buffer, 1, buffer_size, tmp_file) != buffer_size) {
    log_error("Failed to write compressed data to temporary file");
    fclose(tmp_file);
    return -1;
  }
  rewind(tmp_file);

  gzFile gzfile = gzdopen(dup(fileno(tmp_file)), "rb");
  if (!gzfile) {
    log_error("Failed to open gzip file");
    fclose(tmp_file);
    return -1;
  }
  log_trace("gzfile: %p", gzfile);

  int bytes_read = gzread(gzfile, to_buffer, to_buffer_size);
  if (bytes_read < 0) {
    int err;
    const char *error_string = gzerror(gzfile, &err);
    log_error("Failed to read from gzip file: %s", error_string);
    gzclose(gzfile);
    fclose(tmp_file);
    return -1;
  }
  log_trace("bytes_read: %d", bytes_read);

  if (gzclose(gzfile) != Z_OK) {
    log_error("Failed to close gzip file");
    fclose(tmp_file);
    return -1;
  }
  fclose(tmp_file);

  return bytes_read;
}

int compress_deflate(const char *buffer, size_t buffer_size, char *to_buffer) {
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

  log_trace("Original size: %lu, Deflate Compressed size: %lu", buffer_size,
            defstream.total_out);

  return defstream.total_out;
}

int decompress_deflate(const char *buffer, size_t buffer_size, char *to_buffer,
                       size_t to_buffer_size) {
  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;

  infstream.avail_in = (uInt)buffer_size;
  infstream.next_in = (Bytef *)buffer;
  infstream.avail_out = (uInt)to_buffer_size;
  infstream.next_out = (Bytef *)to_buffer;

  inflateInit(&infstream);
  inflate(&infstream, Z_FINISH);
  inflateEnd(&infstream);

  log_trace("Compressed size: %lu, Decompressed size: %lu", buffer_size,
            infstream.total_out);

  return infstream.total_out;
}
