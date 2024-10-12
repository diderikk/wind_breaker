#ifndef FILE_H
#define FILE_H

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int find_static_file(char *uri);

int read_static_file(char *file_name, char *buffer, size_t buffer_size);

int write_static_file(char *file_name, char *buffer, size_t buffer_size);

#endif // FILE_H
