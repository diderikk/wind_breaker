#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int compress_gzip(char *buffer, size_t buffer_size, char *to_buffer);
int compress_deflate(char *buffer, size_t buffer_size, char *to_buffer);

#endif // COMPRESSION_H
