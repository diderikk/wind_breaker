#ifndef STOP_H
#define STOP_H

#include <signal.h>

sig_atomic_t is_shutdown_requested();
void _request_shutdown(const char *file);

#define request_shutdown() _request_shutdown(__FILE__)

#endif // STOP_H
