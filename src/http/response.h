#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include "../static.h"

unsigned int construct_response(http_response_t *http_response,
                                const uri_token_t uri,
                                const char *accept_encoding,
                                const char *if_none_match, buffer *body,
                                buffer *tmp_buffer);
unsigned int construct_upgrade_to_https_response(const uri_token_t uri,
                                                 const char *host,
                                                 buffer *buffer);
char *http_status_code_to_str(HTTP_STATUS_CODE status_code);
#endif // HTTP_RESPONSE_H
