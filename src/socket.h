#ifndef SOCKET_H
#define SOCKET_H

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>

typedef enum { SOCKET, BIND, LISTEN, ACCEPT, CONNECT, SEND, RECV } ERROR_TYPE;

typedef enum { SHOULD_NOT_EXIT, SHOULD_EXIT } EXIT_ACTION;

int open_socket(struct addrinfo *server_info);
int bind_socket(int socket_fd, struct addrinfo *server_info);
int listen_socket(int socket_fd, int backlog_size);
int accept_socket(int socket_fd, struct sockaddr *in_addr);
int connect_socket(int socket_fd, struct sockaddr *in_addr, size_t addr_length);
int send_socket(int socket_fd, char *buffer, size_t buffer_size,
                EXIT_ACTION should_exit);
int recv_socket(int socket_fd, char *buffer, size_t buffer_size,
                EXIT_ACTION should_exit);
void *get_in_addr(struct sockaddr *sa);
int get_in_addr_port(struct sockaddr *sa);
void get_in_addr_str(struct sockaddr *sa, char *buffer, size_t length);

#endif // SOCKET_H
