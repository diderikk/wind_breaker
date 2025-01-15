#ifndef PROPERTIES_H
#define PROPERTIES_H

#include "utils/logger.h"

void init_properties(int argc, char *argv[]);
int get_poll_array_max_size();
int get_session_max_size();
int get_queue_max_size();
int get_worker_thread_max_size();
int get_listen_backlog_max_size();
const char *get_http_port();
const char *get_https_port();
const char *get_env();
const char *get_log_file();
LOG_LEVEL get_log_level();
LOG_DESTINATION get_log_type();

#endif // PROPERTIES_H
