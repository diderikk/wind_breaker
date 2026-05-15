#ifndef STATIC_H
#define STATIC_H

#include "data_structures/buffer.h"
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <pthread.h>

#define HTTP_HEADER_SIZE 256
#define HTTP_URI_TOKEN_COUNT 4
#define HTTP_URI_TOKEN_SIZE 200
#define HTTP_VERSION_SIZE 16
#define HTTP_METHOD_SIZE 8
#define HTTP_HEADER_SMALL_SIZE 128
#define HTTP_HEADER_ETAG_SIZE 65
#define HTTP_VERSION "1.1"
#define BACKTRACE_SIZE 128
#define WB_SQLITE_OPEN_FLAGS                                                   \
  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI |               \
      SQLITE_OPEN_NOMUTEX
#define ALLOCATE_MEMORY_ERROR -123

typedef enum { RESET, REMOVE_FD, CONTINUE } POLL_ERROR_CLASS;

typedef enum {
  HTTP_POST,
  HTTP_GET,
  HTTP_PATCH,
  HTTP_PUT,
  HTTP_HEAD,
  HTTP_DELETE,
  HTTP_CONNECT,
  HTTP_OPTIONS,
  HTTP_TRACE,
  HTTP_BAD_METHOD
} HTTP_METHOD;

typedef enum { KEEP_ALIVE, CLOSE } HTTP_CONNECTION;

typedef enum {
  HTTP_OK = 200,
  HTTP_MOVED_PERMANENTLY = 301,
  HTTP_NOT_MODIFIED = 304,
  HTTP_BAD_REQUEST = 400,
  HTTP_NOT_FOUND = 404,
  HTTP_METHOD_NOT_ALLOWED = 405,
  HTTP_NOT_ACCEPTABLE = 406,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED = 501,
  HTTP_SERVICE_UNAVAILABLE = 503
} HTTP_STATUS_CODE;

typedef enum {
  WORK_STATUS_INITIAL = 1,
  WORK_STATUS_REQUEST_READ = 1 << 1,
  WORK_STATUS_PARSED = 1 << 2,
  WORK_STATUS_DATA_FETCHED = 1 << 3,
  WORK_STATUS_READY_TO_SEND = 1 << 4,
  WORK_STATUS_SEND_FAILED = 1 << 5,
  WORK_STATUS_SENT = 1 << 6,
  WORK_STATUS_PROCESSING = 1 << 7,
  WORK_STATUS_REJECTED = 1 << 8,
  WORK_STATUS_INITIAL_SSL = 1 << 9
} WORK_STATUS;

typedef struct {
  int fd;
  int size;
  char *data;
} worker_data_t;

typedef struct {
  void *arg;
} worker_arg_t;

// TODO:
// Upgrade-Insecure-Requests: 1
// Sec-Fetch-Dest: document
// Sec-Fetch-Mode: navigate
// Sec-Fetch-Site: none
// Sec-Fetch-User: ?1
// Priority: u=0, i

typedef struct {
  HTTP_STATUS_CODE status_code;
  char content_type[HTTP_HEADER_SIZE];
  long content_length;
  char content_language[HTTP_HEADER_SMALL_SIZE];
  char content_encoding[HTTP_HEADER_SMALL_SIZE];
  char last_modified[HTTP_HEADER_SMALL_SIZE];
  char date[HTTP_HEADER_SMALL_SIZE];
  char etag[HTTP_HEADER_ETAG_SIZE];
  char location[HTTP_HEADER_SIZE];
} http_response_t;

typedef char uri_token_t[HTTP_URI_TOKEN_COUNT][HTTP_URI_TOKEN_SIZE];

typedef struct {
  uri_token_t uri;
  HTTP_METHOD method;
  char is_ssl;
  char version[HTTP_VERSION_SIZE];
  char host[HTTP_HEADER_SIZE];
  char user_agent[HTTP_HEADER_SIZE];
  char accept[HTTP_HEADER_SIZE];
  char accept_language[HTTP_HEADER_SIZE];
  char accept_encoding[HTTP_HEADER_SIZE];
  char if_none_match[HTTP_HEADER_ETAG_SIZE];
  HTTP_CONNECTION connection;
  char content_type[HTTP_HEADER_SIZE];
  long content_length;
} http_request_t;

typedef struct {
  worker_data_t **data;
  int count, front, rear;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} queue_t;

typedef struct {
  int id;
  int related_fd;
} meta_t;

typedef struct {
  meta_t meta;
  BIO *bio;
  SSL *ssl;
  buffer *buffer;
  http_request_t request;
  http_response_t response;
  unsigned char failed_send_attempts;
} session_t;

struct session_node {
  struct session_node *next;
  struct session_node *tail;
  session_t session;
};

typedef struct session_node session_node_t;

char *uri_to_file_name(const uri_token_t uri);

#endif // STATIC_H
