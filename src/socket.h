#ifndef SOCKET_H
#define SOCKET_H

#include "static.h"
#include <netdb.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <sys/socket.h>

int open_socket(const struct addrinfo *server_info);
int bind_socket(int socket_fd, const struct addrinfo *server_info);
int listen_socket(int socket_fd, int backlog_size);
int accept_socket(int socket_fd, struct sockaddr *in_addr);
int connect_socket(int socket_fd, const struct sockaddr *in_addr,
                   size_t addr_length);
int send_ssl(SSL *ssl, const buffer *buffer);
int send_bio(BIO *bio, const buffer *buffer);
int send_socket(int socket_fd, const char *buffer, size_t buffer_size);
int recv_ssl(SSL *ssl, buffer *buffer);
int recv_bio(BIO *bio, buffer *buffer);
int recv_socket(int socket_fd, char *buffer, size_t buffer_size);
void *get_in_addr(const struct sockaddr *sa);
int get_in_addr_port(const struct sockaddr *sa);
void get_in_addr_str(const struct sockaddr *sa, char *buffer, size_t length);

#endif // SOCKET_H
