#ifndef STATIC_FILE_H
#define STATIC_FILE_H

#include <fcntl.h>
#include <unistd.h>

int find_static_file(char *uri);

int read_static_file(char *file_name, char *buffer, size_t buffer_size);

int write_static_file(char *file_name, char *buffer, size_t buffer_size);

#endif // STATIC_FILE_H
