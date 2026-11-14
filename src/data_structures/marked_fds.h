#ifndef MARKED_FDS_H
#define MARKED_FDS_H

#define MAX_FD 16384

void init_marked_fds();
void destroy_marked_fds();

void mark(int fd);
void unmark(int fd);
char is_marked(int fd);

#endif // MARKED_FDS_H
