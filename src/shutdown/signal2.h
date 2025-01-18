#ifndef SIGNAL2_H
#define SIGNAL2_H

#include <signal.h>
#include <stdio.h>

void handle_signals(int socket_fd1, int socket_fd2, const FILE *log_file);

#endif // SIGNAL2_H
