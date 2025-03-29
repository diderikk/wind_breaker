#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

unsigned int construct_response(int response_code, const char *uri,
                                const char *accept_encoding,
                                const char *if_none_match, char *response,
                                char *tmp_body_buffer);
unsigned int construct_upgrade_to_https_response(const char *uri,
                                                 const char *host,
                                                 char *response);
#endif // HTTP_RESPONSE_H
