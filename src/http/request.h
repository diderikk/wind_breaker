#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "../static.h"

int parse_http_request(http_request_t *http_request, const char *char_data);
int validate_request_headers(const http_request_t *http_request);

#endif // HTTP_REQUEST_H
