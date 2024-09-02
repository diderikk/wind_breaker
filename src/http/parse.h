#ifndef HTTP_PARSE_H
#define HTTP_PARSE_H
#define HTTP_HEADER_SIZE 1024
#define HTTP_URI_SIZE 512
#define HTTP_VERSION_SIZE 16
#define HTTP_METHOD_SIZE 16
#define HTTP_BODY_SIZE 8096

#include <regex.h>

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

typedef enum {
    KEEP_ALIVE,
    CLOSE
} http_connection;

// TODO:
// Upgrade-Insecure-Requests: 1
// Sec-Fetch-Dest: document
// Sec-Fetch-Mode: navigate
// Sec-Fetch-Site: none
// Sec-Fetch-User: ?1
// Priority: u=0, i

typedef struct {
    char uri[HTTP_URI_SIZE];
    http_method method;
    char version[HTTP_VERSION_SIZE];
    char host[HTTP_HEADER_SIZE];
    char user_agent [HTTP_HEADER_SIZE];
    char accept [HTTP_HEADER_SIZE];
    char accept_language[HTTP_HEADER_SIZE];
    char accept_encoding[HTTP_HEADER_SIZE];
    http_connection connection;
    char content_type[HTTP_HEADER_SIZE];
    long content_length;

} http_request_t;

int parse_http_request(http_request_t* http_request, char * char_data);

#endif // HTTP_PARSE_H
