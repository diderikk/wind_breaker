#ifndef COMPRESSION_H
#define COMPRESSION_H

int compress_gzip(const char *buffer, unsigned int buffer_size,
                  char *to_buffer);
int decompress_gzip(const char *buffer, unsigned int buffer_size,
                    char *to_buffer, unsigned int to_buffer_size);
int compress_deflate(const char *buffer, unsigned int buffer_size,
                     char *to_buffer);
int decompress_deflate(const char *buffer, unsigned int buffer_size,
                       char *to_buffer, unsigned int to_buffer_size);

#endif // COMPRESSION_H
