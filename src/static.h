#ifndef STATIC_H
#define STATIC_H

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <pthread.h>

#define DEFAULT_BUFFER_SIZE 1024
#define HTML_MAX_SIZE 70 * 1024               // 70 kB
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
} http_method;

typedef enum { KEEP_ALIVE, CLOSE } http_connection;

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
} http_status_code;

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
  char* data;
} worker_data;

typedef struct {
  void *arg;
} worker_arg;

// TODO:
// Upgrade-Insecure-Requests: 1
// Sec-Fetch-Dest: document
// Sec-Fetch-Mode: navigate
// Sec-Fetch-Site: none
// Sec-Fetch-User: ?1
// Priority: u=0, i

typedef struct {
  http_status_code status_code;
  char content_type[HTTP_HEADER_SIZE];
  long content_length;
  long offset;
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
  http_method method;
  char is_ssl;
  char version[HTTP_VERSION_SIZE];
  char host[HTTP_HEADER_SIZE];
  char user_agent[HTTP_HEADER_SIZE];
  char accept[HTTP_HEADER_SIZE];
  char accept_language[HTTP_HEADER_SIZE];
  char accept_encoding[HTTP_HEADER_SIZE];
  char if_none_match[HTTP_HEADER_ETAG_SIZE];
  http_connection connection;
  char content_type[HTTP_HEADER_SIZE];
  long content_length;
} http_request_t;

typedef struct {
  worker_data **data;
  int count, front, rear;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} queue_t;

typedef struct {
  unsigned long capacity;
  unsigned long count;
  char *data;
} buffer;

struct session {
  int id;
  int related_fd;
  unsigned long thread_id;
};

struct session_full_return {
  struct session *session;
  BIO *bio;
  SSL *ssl;
  buffer *buffer;
  http_request_t *request;
  http_response_t *response;
};

#define ENSURE_CAPACITY(buffer, capacity) (__FILE__, buffer, capacity)

int ensure_capacity_(const char* callee, buffer* buffer, unsigned long capacity);
char *uri_to_file_name(const uri_token_t uri);


#endif // STATIC_H
