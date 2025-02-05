#ifndef SIGNAL2_H
#define SIGNAL2_H

#include <stdio.h>

void handle_signals(void (*handle_exit)(int));
char *get_signal_description(int signum);

#endif // SIGNAL2_H
