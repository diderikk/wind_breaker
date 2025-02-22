#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

unsigned int construct_response(int response_code, const char *uri,
                                const char *accept_encoding,
                                const char *if_none_match, char *response,
                                char *tmp_body_buffer);
#endif // HTTP_RESPONSE_H
