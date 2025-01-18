#ifndef STOP_H
#define STOP_H

#include <signal.h>

sig_atomic_t stop();
void _stop_server(const char *file);

#define stop_server() _stop_server(__FILE__)

#endif // STOP_H
