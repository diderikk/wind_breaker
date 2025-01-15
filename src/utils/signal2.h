#ifndef SIGNAL2_H
#define SIGNAL2_H

#include <signal.h>
#include <stdio.h>

sig_atomic_t stop();
void handle_signals(int socket_fd1, int socket_fd2, FILE *log_file);

#endif // SIGNAL2_H
