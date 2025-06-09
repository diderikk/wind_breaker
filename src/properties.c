#include "properties.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_BUFFER_SIZE 4096

static int session_max_size = 64;
static int worker_thread_max_size = 1;
static int listen_backlog_max_size = 50;
static char http_port[5] = "8080";
static char https_port[5] = "8443";
static char env[4] = "dev";
static char log_file[200] = "/tmp/app.log";
static int log_level = 0;
static int log_type = 0;
static char properties_file[200] = "/etc/wind_breaker/properties.conf";
static char cert_file[200] = "/etc/wind_breaker/cert.pem";
static char private_key_file[200] = "/etc/wind_breaker/key.pem";
static char db_url[200] = "/etc/wind_breaker/data.db";
static char reset_db = 0;
static char enable_https = 0;

void init_properties(int argc, char *argv[]) {
  char log_buffer[LOG_BUFFER_SIZE + 1];
  FILE *f;
  int read_size, log_buffer_offset;
  char *line = NULL;
  size_t read_len = 0;
  log_buffer_offset = 0;

  if (argc > 1)
    log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                  LOG_BUFFER_SIZE - log_buffer_offset,
                                  "Running with %d arguments\n", argc);
  // First arg is the program name
  for (int i = 1; i < argc; i++) {
    if ((strcasecmp(argv[i], "-p") == 0 ||
         strcasecmp(argv[i], "--port") == 0) &&
        i + 1 <= argc) {
      // TODO: Use strtol and check for errors
      assert(atoi(argv[i + 1]) > 0 && atoi(argv[i + 1]) < 65536);
      strcpy(http_port, argv[i + 1]);
      log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                    LOG_BUFFER_SIZE - log_buffer_offset,
                                    "Port from args: %s\n", http_port);
    } else if ((strcasecmp(argv[i], "-e") == 0 ||
                strcasecmp(argv[i], "--environment") == 0) &&
               i + 1 <= argc) {
      assert(strcasecmp(argv[i + 1], "dev") == 0 ||
             strcasecmp(argv[i + 1], "prod") == 0 ||
             strcasecmp(argv[i + 1], "test") == 0);
      strcpy(env, argv[i + 1]);
      log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                    LOG_BUFFER_SIZE - log_buffer_offset,
                                    "Environment from args: %s\n", env);
    } else if (strcasecmp(argv[i], "--properties") == 0 && i + 1 <= argc) {
      assert(access(argv[i + 1], F_OK) == 0);
      strcpy(properties_file, argv[i + 1]);
      log_buffer_offset += snprintf(
          log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
          "Properties file from args: %s\n", properties_file);
    } else if (strcasecmp(argv[i], "--log-file") == 0 && i + 1 <= argc) {
      strcpy(log_file, argv[i + 1]);
      log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                    LOG_BUFFER_SIZE - log_buffer_offset,
                                    "Log file from args: %s\n", log_file);
    } else if (strcasecmp(argv[i], "--log-level") == 0 && i + 1 <= argc) {
      assert(strcasecmp(argv[i + 1], "TRACE") == 0 ||
             strcasecmp(argv[i + 1], "DEBUG") == 0 ||
             strcasecmp(argv[i + 1], "INFO") == 0 ||
             strcasecmp(argv[i + 1], "WARN") == 0 ||
             strcasecmp(argv[i + 1], "ERROR") == 0 ||
             strcasecmp(argv[i + 1], "FATAL") == 0);
      if (strcasecmp(argv[i + 1], "TRACE") == 0) {
        log_level = 0;
      } else if (strcasecmp(argv[i + 1], "DEBUG") == 0) {
        log_level = 1;
      } else if (strcasecmp(argv[i + 1], "INFO") == 0) {
        log_level = 2;
      } else if (strcasecmp(argv[i + 1], "WARN") == 0) {
        log_level = 3;
      } else if (strcasecmp(argv[i + 1], "ERROR") == 0) {
        log_level = 4;
      }
      log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                    LOG_BUFFER_SIZE - log_buffer_offset,
                                    "Log level from args: %d\n", log_level);
    } else if (strcasecmp(argv[i], "--log-type") == 0 && i + 1 <= argc) {
      assert(strcasecmp(argv[i + 1], "CONSOLE_ONLY") == 0 ||
             strcasecmp(argv[i + 1], "FILE_ONLY") == 0 ||
             strcasecmp(argv[i + 1], "CONSOLE_FILE") == 0);
      if (strcasecmp(argv[i + 1], "CONSOLE_ONLY") == 0) {
        log_type = 0;
      } else if (strcasecmp(argv[i + 1], "FILE_ONLY") == 0) {
        log_type = 1;
      } else if (strcasecmp(argv[i + 1], "CONSOLE_FILE") == 0) {
        log_type = 2;
      }
      log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                    LOG_BUFFER_SIZE - log_buffer_offset,
                                    "Log type from args: %d\n", log_type);
    } else if (strcasecmp(argv[i], "--reset-db") == 0) {
      reset_db = 1;
    } else if (strcasecmp(argv[i], "--enable-https") == 0) {
      enable_https = 1;
      log_buffer_offset +=
          snprintf(log_buffer + log_buffer_offset,
                   LOG_BUFFER_SIZE - log_buffer_offset, "Enabling HTTPS\n");
    }
  }

  f = fopen(properties_file, "r");
  if (f != NULL) {
    log_buffer_offset += snprintf(
        log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
        "Using properties file: %s\n", properties_file);

    while ((read_size = getline(&line, &read_len, f)) != -1) {
      line[read_size - 1] = '\0';
      if (strncasecmp(line, "session_max_size", 16) == 0) {
        assert(atoi(line + 17) > 0 && atoi(line + 17) < 65536);
        session_max_size = atoi(line + 17);
        log_buffer_offset += snprintf(
            log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
            "Session max size: %d\n", session_max_size);
      } else if (strncasecmp(line, "worker_thread_max_size", 22) == 0) {
        assert(atoi(line + 23) > 0 && atoi(line + 23) < 65536);
        worker_thread_max_size = atoi(line + 23);
        log_buffer_offset += snprintf(
            log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
            "Worker thread max size: %d\n", worker_thread_max_size);
      } else if (strncasecmp(line, "listen_backlog_max_size", 23) == 0) {
        assert(atoi(line + 24) > 0 && atoi(line + 24) < 65536);
        listen_backlog_max_size = atoi(line + 24);
        log_buffer_offset += snprintf(
            log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
            "Listen backlog max size: %d\n", listen_backlog_max_size);
      } else if (strncasecmp(line, "http_port", 9) == 0) {
        assert(atoi(line + 10) > 0 && atoi(line + 10) < 65536);
        strcpy(http_port, line + 10);
        log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                      LOG_BUFFER_SIZE - log_buffer_offset,
                                      "HTTP port: %s\n", http_port);
      } else if (strncasecmp(line, "env", 3) == 0) {
        assert(strncasecmp(line + 4, "dev", 3) == 0 ||
               strncasecmp(line + 4, "prod", 4) == 0 ||
               strncasecmp(line + 4, "test", 4) == 0);
        strcpy(env, line + 4);
        log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                      LOG_BUFFER_SIZE - log_buffer_offset,
                                      "Environment: %s\n", env);
      } else if (strncasecmp(line, "log_file", 8) == 0) {
        // Created if missing
        strcpy(log_file, line + 9);
        log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                      LOG_BUFFER_SIZE - log_buffer_offset,
                                      "Log file: %s\n", log_file);
      } else if (strncasecmp(line, "log_level", 9) == 0) {
        assert(strcasecmp(line + 10, "TRACE") == 0 ||
               strcasecmp(line + 10, "DEBUG") == 0 ||
               strcasecmp(line + 10, "INFO") == 0 ||
               strcasecmp(line + 10, "WARN") == 0 ||
               strcasecmp(line + 10, "ERROR") == 0 ||
               strcasecmp(line + 10, "FATAL") == 0);
        if (strcasecmp(line + 10, "TRACE") == 0) {
          log_level = 0;
        } else if (strcasecmp(line + 10, "DEBUG") == 0) {
          log_level = 1;
        } else if (strcasecmp(line + 10, "INFO") == 0) {
          log_level = 2;
        } else if (strcasecmp(line + 10, "WARN") == 0) {
          log_level = 3;
        } else if (strcasecmp(line + 10, "ERROR") == 0) {
          log_level = 4;
        }
        log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                      LOG_BUFFER_SIZE - log_buffer_offset,
                                      "Log level: %d\n", log_level);
      } else if (strncasecmp(line, "log_type", 8) == 0) {
        assert(strcasecmp(line + 9, "CONSOLE_ONLY") == 0 ||
               strcasecmp(line + 9, "FILE_ONLY") == 0 ||
               strcasecmp(line + 9, "CONSOLE_FILE") == 0);
        if (strcasecmp(line + 9, "CONSOLE_ONLY") == 0) {
          log_type = 0;
        } else if (strcasecmp(line + 9, "FILE_ONLY") == 0) {
          log_type = 1;
        } else if (strcasecmp(line + 9, "CONSOLE_FILE") == 0) {
          log_type = 2;
        }
        log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                      LOG_BUFFER_SIZE - log_buffer_offset,
                                      "Log type: %d\n", log_type);
      } else if (strncasecmp(line, "cert_chain_file", 16) == 0) {
        strcpy(cert_file, line + 17);
        log_buffer_offset += snprintf(
            log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
            "Certificate chain file: %s\n", cert_file);
      } else if (strncasecmp(line, "private_key_file", 16) == 0) {
        strcpy(private_key_file, line + 17);
        log_buffer_offset += snprintf(
            log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
            "Private key file: %s\n", private_key_file);
      } else if (strncasecmp(line, "enable_https", 12) == 0) {
        enable_https = atoi(line + 13) > 0;
        log_buffer_offset += snprintf(log_buffer + log_buffer_offset,
                                      LOG_BUFFER_SIZE - log_buffer_offset,
                                      "Enable HTTPS: %d\n", enable_https);
      }
    }

    fclose(f);
    free(line);
  } else {
    log_buffer_offset += snprintf(
        log_buffer + log_buffer_offset, LOG_BUFFER_SIZE - log_buffer_offset,
        "Failed to find properties file, using default values with port %s and "
        "environment %s\n",
        http_port, env);
  }

  if (strcasecmp(env, "dev") == 0) {
    log_buffer[LOG_BUFFER_SIZE] = '\0';
    printf("%s", log_buffer);
  }
}

int get_session_max_size() { return session_max_size; }

void set_session_max_size(int size) { session_max_size = size; }

int get_worker_thread_max_size() { return worker_thread_max_size; }

int get_listen_backlog_max_size() { return listen_backlog_max_size; }

const char *get_http_port() { return http_port; }

const char *get_https_port() { return https_port; }

const char *get_env() { return env; }

void set_env(char *env) {
  assert(strncasecmp(env, "dev", 3) == 0 || strncasecmp(env, "prod", 4) == 0 ||
         strncasecmp(env, "test", 4) == 0);

  strcpy(env, env);
}

const char *get_log_file() { return log_file; }

int get_log_level() { return log_level; }

int get_log_type() { return log_type; }

const char *get_cert_file() { return cert_file; }

const char *get_key_file() { return private_key_file; }

const char *get_db_url() { return db_url; }

char get_reset_db() { return reset_db; }

char get_enabled_ssl() { return enable_https; }
