#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int find_static_file(char *uri);

int read_static_file(char *uri, char *buffer, size_t buffer_size);

#endif // FILE_H
