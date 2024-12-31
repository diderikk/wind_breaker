#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int compress_gzip(const char *buffer, size_t buffer_size, char *to_buffer);
int decompress_gzip(const char *buffer, size_t buffer_size, char *to_buffer,
                    size_t to_buffer_size);
int compress_deflate(const char *buffer, size_t buffer_size, char *to_buffer);
int decompress_deflate(const char *buffer, size_t buffer_size, char *to_buffer,
                       size_t to_buffer_size);

#endif // COMPRESSION_H
