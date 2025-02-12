#ifndef PROPERTIES_H
#define PROPERTIES_H

#include "utils/logger.h"

void init_properties(int argc, char *argv[]);
int get_session_max_size();
void set_session_max_size(int size);
int get_queue_max_size();
void set_queue_max_size(int size);
int get_worker_thread_max_size();
int get_listen_backlog_max_size();
const char *get_http_port();
const char *get_https_port();
const char *get_env();
void set_env(char *env);
const char *get_log_file();
LOG_LEVEL get_log_level();
LOG_DESTINATION get_log_type();
char *get_cert_file();
char *get_key_file();

#endif // PROPERTIES_H
