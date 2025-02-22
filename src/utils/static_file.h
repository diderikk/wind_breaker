#ifndef STATIC_FILE_H
#define STATIC_FILE_H

int find_static_file(const char *uri);
int read_static_file(const char *file_name, char *buffer,
                     unsigned int buffer_size);
int write_static_file(const char *file_name, char *buffer,
                      unsigned int buffer_size);
int get_last_modified(const char *file_path);

#endif // STATIC_FILE_H
