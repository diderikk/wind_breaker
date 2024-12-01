#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "static.h"
#include <regex.h>

int parse_request(http_request_t *http_request, char *raw_request);
int parse_http_request(http_request_t *http_request, char *char_data);

#endif // HTTP_REQUEST_H
