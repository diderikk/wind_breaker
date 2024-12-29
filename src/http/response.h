#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "../utils/compression.h"
#include "../utils/static_file.h"
#include "static.h"

size_t construct_response(int response_code, char *uri, char *accept_encoding,
                          char *if_none_match, char *response,
                          char *tmp_body_buffer);
#endif // HTTP_RESPONSE_H
