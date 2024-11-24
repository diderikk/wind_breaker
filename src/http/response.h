#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "../utils/compression.h"
#include "../utils/file.h"
#include "static.h"

size_t construct_response(http_request_t *http_request, char *response);
#endif // HTTP_RESPONSE_H
