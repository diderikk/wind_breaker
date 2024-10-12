#ifndef HTTP_STATIC_H
#define HTTP_STATIC_H

#define HTTP_HEADER_SIZE 1024
#define HTTP_URI_SIZE 512
#define HTTP_VERSION_SIZE 16
#define HTTP_METHOD_SIZE 16
#define HTTP_BODY_SIZE 8096
#define HTTP_HEADER_SMALL_SIZE 128
#define HTTP_VERSION "1.1"

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

// TODO:
// Upgrade-Insecure-Requests: 1
// Sec-Fetch-Dest: document
// Sec-Fetch-Mode: navigate
// Sec-Fetch-Site: none
// Sec-Fetch-User: ?1
// Priority: u=0, i

typedef enum {
  HTTP_OK = 200,
  HTTP_BAD_REQUEST = 400,
  HTTP_NOT_FOUND = 404,
  HTTP_METHOD_NOT_ALLOWED = 405,
  HTTP_NOT_ACCEPTABLE = 406,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED = 501,
  HTTP_SERVICE_UNAVAILABLE = 503
} http_status_code;

typedef struct {
  http_status_code status_code;
  char content_type[HTTP_HEADER_SIZE];
  long content_length;
  char content_language[HTTP_HEADER_SMALL_SIZE];
  char body[HTTP_BODY_SIZE];
} http_response_t;

typedef struct {
  char uri[HTTP_URI_SIZE];
  http_method method;
  char version[HTTP_VERSION_SIZE];
  char host[HTTP_HEADER_SIZE];
  char user_agent[HTTP_HEADER_SIZE];
  char accept[HTTP_HEADER_SIZE];
  char accept_language[HTTP_HEADER_SIZE];
  char accept_encoding[HTTP_HEADER_SIZE];
  http_connection connection;
  char content_type[HTTP_HEADER_SIZE];
  long content_length;
} http_request_t;

#endif // HTTP_STATIC_H
