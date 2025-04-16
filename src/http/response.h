#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include "../static.h"

unsigned int construct_response(http_response_t *http_response, const char *uri,
                                const char *accept_encoding,
                                const char *if_none_match, char *body_buffer,
                                unsigned int body_size);
unsigned int construct_upgrade_to_https_response(const char *uri,
                                                 const char *host,
                                                 char *response);
char *http_status_code_to_str(http_status_code status_code);
#endif // HTTP_RESPONSE_H
