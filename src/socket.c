#include "socket.h"
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "string.h"

void error(ERROR_TYPE code, int socket_fd, EXIT_ACTION should_exit);
const char* handle_socket_error(int code);
const char* handle_bind_error(int code);
const char* handle_listen_error(int code);
const char* handle_accept_error(int code);
const char* handle_connect_error(int code);
const char* handle_send_error(int code);
const char* handle_recv_error(int code);

int open_socket(struct addrinfo * server_info){
    int socket_fd;

    socket_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

    if(socket_fd == -1) {
        error(SOCKET, socket_fd, SHOULD_NOT_EXIT);
    }

    return socket_fd;
}


int bind_socket(int socket_fd, struct addrinfo * server_info){
    int bind_result;

    bind_result = bind(socket_fd, (struct sockaddr*)server_info->ai_addr, server_info->ai_addrlen);

    if(bind_result < 0){
        error(BIND, socket_fd, SHOULD_NOT_EXIT);
    }

    return bind_result;
}

int listen_socket(int socket_fd, int backlog_size){
    int listen_result;

    listen_result = listen(socket_fd, backlog_size);

    if(listen_result < 0){
        error(LISTEN, socket_fd, SHOULD_EXIT);
    }

    return listen_result;
}

int accept_socket(int socket_fd, struct sockaddr * in_addr){
    int client_socket_fd;
    socklen_t in_addr_size;

    in_addr_size = sizeof(*in_addr);
    client_socket_fd = accept(socket_fd, in_addr, &in_addr_size);
    
    if(client_socket_fd < 0){
        error(ACCEPT, socket_fd, SHOULD_EXIT);
    }
   
    return client_socket_fd;
}

int connect_socket(int socket_fd, struct sockaddr * in_addr, size_t addr_length){
    int connect_return;

    connect_return = connect(socket_fd, in_addr, addr_length);
    
    if(connect_return < 0){
        error(CONNECT, socket_fd, SHOULD_NOT_EXIT);
    }
   
    return connect_return;
}

int send_socket(int socket_fd, char* data, EXIT_ACTION should_exit){
    int send_return;

    send_return = send(socket_fd, data, strlen(data), 0);
    if (send_return < 0) {
        error(SEND, socket_fd, should_exit);
    }

    return send_return;
}

int recv_socket(int socket_fd, char* buffer, size_t buffer_size, EXIT_ACTION should_exit){
    int recv_return;

    memset(buffer, 0, buffer_size);
    recv_return = recv(socket_fd, buffer, buffer_size - 1, 0);
    if (recv_return < 0) {
        error(RECV, socket_fd, should_exit);
    }
    buffer[buffer_size] = '\0';

    return recv_return;
}

// TODO: Move to separate file
// https://beej.us/guide/bgnet/html/split/client-server-background.html
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_in_addr_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)sa)->sin_port);
    }

    return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

void get_in_addr_str(struct sockaddr *sa, char* buffer, size_t length)
{
    inet_ntop(sa->sa_family,
            get_in_addr(sa),
            buffer, length);
}


void error(ERROR_TYPE code, int socket_fd, EXIT_ACTION should_exit){
    switch (code) {
        case SOCKET:
            perror(handle_socket_error(errno));
            break;
        case BIND:
            perror(handle_bind_error(errno));
            break;
        case LISTEN:
            perror(handle_listen_error(errno));
            break;
        case ACCEPT:
            perror(handle_accept_error(errno));
            break;
        case CONNECT:
            perror(handle_connect_error(errno));
            break;
        case SEND:
            perror(handle_send_error(errno));
            break;
        case RECV:
            perror(handle_recv_error(errno));
            break;
    }
    if(socket_fd != -1)
        close(socket_fd);
    if(should_exit == SHOULD_EXIT)
        exit(EXIT_FAILURE);
}

const char* handle_socket_error(int code) {
    switch (code) {
        case EACCES: return "Permission to create a socket of the specified type and/or protocol is denied.";
        case EAFNOSUPPORT: return "The implementation does not support the specified address family.";
        case EINVAL: return "Unknown protocol, or protocol family not available.";
        case EMFILE: return "The per-process limit on the number of open file descriptors has been reached.";
        case ENFILE: return "The system-wide limit on the total number of open files has been reached.";
        case ENOBUFS: return "Insufficient memory is available.  The socket cannot be created until sufficient resources are freed.";
        case ENOMEM: return "Insufficient memory is available.  The socket cannot be created until sufficient resources are freed.";
        case EPROTONOSUPPORT: return "The protocol type or the specified protocol is not supported within this domain.";

        default: return "Unknown error";
    }
}

const char* handle_bind_error(int code) {
    switch(code) {
        case EACCES:
            return "EACCES: The address is protected, and the user is not the superuser.";
        case EADDRINUSE:
            return "EADDRINUSE: The given address is already in use.";
        case EBADF:
            return "EBADF: sockfd is not a valid file descriptor.";
        case EINVAL:
            return "EINVAL: The socket is already bound to an address or addrlen is wrong, or addr is not a valid address for this socket's domain.";
        case ENOTSOCK:
            return "ENOTSOCK: The file descriptor sockfd does not refer to a socket.";
        case EADDRNOTAVAIL:
            return "EADDRNOTAVAIL: A nonexistent interface was requested or the requested address was not local.";
        case EFAULT:
            return "EFAULT: addr points outside the user's accessible address space.";
        case ELOOP:
            return "ELOOP: Too many symbolic links were encountered in resolving addr.";
        case ENAMETOOLONG:
            return "ENAMETOOLONG: addr is too long.";
        case ENOENT:
            return "ENOENT: A component in the directory prefix of the socket pathname does not exist.";
        case ENOMEM:
            return "ENOMEM: Insufficient kernel memory was available.";
        case ENOTDIR:
            return "ENOTDIR: A component of the path prefix is not a directory.";
        case EROFS:
            return "EROFS: The socket inode would reside on a read-only filesystem.";
        default: return "Unknown error"; 
    }
}

const char* handle_listen_error(int code) {
    switch (code) {
        case EADDRINUSE:
            return "EADDRINUSE: Another socket is already listening on the same port or all ephemeral ports are in use.";
        case EBADF:
            return "EBADF: The argument sockfd is not a valid file descriptor.";
        case ENOTSOCK:
            return "ENOTSOCK: The file descriptor sockfd does not refer to a socket.";
        case EOPNOTSUPP:
            return "EOPNOTSUPP: The socket is not of a type that supports the listen() operation.";
        default:
            return "Unknown error";
    }
}

const char* handle_accept_error(int code) {
    switch (code) {
        case EAGAIN: // Same as EWOULDBLOCK
            return "EAGAIN or EWOULDBLOCK: The socket is marked nonblocking and no connections are present to be accepted.";
        case EBADF:
            return "EBADF: sockfd is not an open file descriptor.";
        case ECONNABORTED:
            return "ECONNABORTED: A connection has been aborted.";
        case EFAULT:
            return "EFAULT: The addr argument is not in a writable part of the user address space.";
        case EINTR:
            return "EINTR: The system call was interrupted by a signal that was caught before a valid connection arrived.";
        case EINVAL:
            return "EINVAL: Socket is not listening for connections, or addrlen is invalid.";
        case EMFILE:
            return "EMFILE: The per-process limit on the number of open file descriptors has been reached.";
        case ENFILE:
            return "ENFILE: The system-wide limit on the total number of open files has been reached.";
        case ENOBUFS:
        case ENOMEM:
            return "ENOBUFS or ENOMEM: Not enough free memory.";
        case ENOTSOCK:
            return "ENOTSOCK: The file descriptor sockfd does not refer to a socket.";
        case EOPNOTSUPP:
            return "EOPNOTSUPP: The referenced socket is not of type SOCK_STREAM.";
        case EPERM:
            return "EPERM: Firewall rules forbid connection.";
        case EPROTO:
            return "EPROTO: Protocol error.";
        default:
            return "Unknown error";
    }
}

const char* handle_connect_error(int code) {
    switch (code) {
        case EACCES:
            return "EACCES: Write permission is denied on the socket file, or search permission is denied for one of the directories in the path prefix, or an SELinux policy denied a connection.";
        case EPERM:
            return "EPERM: The user tried to connect to a broadcast address without having the socket broadcast flag enabled or the connection request failed because of a local firewall rule.";
        case EADDRINUSE:
            return "EADDRINUSE: Local address is already in use.";
        case EADDRNOTAVAIL:
            return "EADDRNOTAVAIL: The socket had not previously been bound to an address and all ephemeral ports are in use.";
        case EAFNOSUPPORT:
            return "EAFNOSUPPORT: The passed address didn't have the correct address family in its sa_family field.";
        case EAGAIN:
            return "EAGAIN: The socket is nonblocking, and the connection cannot be completed immediately.";
        case EALREADY:
            return "EALREADY: The socket is nonblocking and a previous connection attempt has not yet been completed.";
        case EBADF:
            return "EBADF: sockfd is not a valid open file descriptor.";
        case ECONNREFUSED:
            return "ECONNREFUSED: No one is listening on the remote address.";
        case EFAULT:
            return "EFAULT: The socket structure address is outside the user's address space.";
        case EINPROGRESS:
            return "EINPROGRESS: The socket is nonblocking and the connection cannot be completed immediately.";
        case EINTR:
            return "EINTR: The system call was interrupted by a signal that was caught.";
        case EISCONN:
            return "EISCONN: The socket is already connected.";
        case ENETUNREACH:
            return "ENETUNREACH: Network is unreachable.";
        case ENOTSOCK:
            return "ENOTSOCK: The file descriptor sockfd does not refer to a socket.";
        case EPROTOTYPE:
            return "EPROTOTYPE: The socket type does not support the requested communications protocol.";
        case ETIMEDOUT:
            return "ETIMEDOUT: Timeout while attempting connection.";
        default:
            return "Unknown error";
    }
}

const char* handle_send_error(int code) {
    switch (code) {
        case EACCES:
            return "EACCES: Write permission is denied on the destination socket file, or search permission is denied for one of the directories in the path prefix. For UDP sockets: An attempt was made to send to a network/broadcast address as though it was a unicast address.";
        case EWOULDBLOCK:
            return "EAGAIN or EWOULDBLOCK: The socket is marked nonblocking and the requested operation would block.";
        case EALREADY:
            return "EALREADY: Another Fast Open is in progress.";
        case EBADF:
            return "EBADF: sockfd is not a valid open file descriptor.";
        case ECONNRESET:
            return "ECONNRESET: Connection reset by peer.";
        case EDESTADDRREQ:
            return "EDESTADDRREQ: The socket is not connection-mode, and no peer address is set.";
        case EFAULT:
            return "EFAULT: An invalid user space address was specified for an argument.";
        case EINTR:
            return "EINTR: A signal occurred before any data was transmitted.";
        case EINVAL:
            return "EINVAL: Invalid argument passed.";
        case EISCONN:
            return "EISCONN: The connection-mode socket was connected already but a recipient was specified.";
        case EMSGSIZE:
            return "EMSGSIZE: The socket type requires that message be sent atomically, and the size of the message to be sent made this impossible.";
        case ENOBUFS:
            return "ENOBUFS: The output queue for a network interface was full.";
        case ENOMEM:
            return "ENOMEM: No memory available.";
        case ENOTCONN:
            return "ENOTCONN: The socket is not connected, and no target has been given.";
        case ENOTSOCK:
            return "ENOTSOCK: The file descriptor sockfd does not refer to a socket.";
        case EOPNOTSUPP:
            return "EOPNOTSUPP: Some bit in the flags argument is inappropriate for the socket type.";
        case EPIPE:
            return "EPIPE: The local end has been shut down on a connection-oriented socket. The process will also receive a SIGPIPE unless MSG_NOSIGNAL is set.";
        default:
            return "Unavailble error";
    }
}

const char* handle_recv_error(int code) {
    switch (code) {
        case EAGAIN:
            return "EAGAIN or EWOULDBLOCK: The socket is marked nonblocking and the receive operation would block, or a receive timeout had been set and the timeout expired before data was received. POSIX.1 allows either error to be returned for this case, and does not require these constants to have the same value, so a portable application should check for both possibilities.";
        case EBADF:
            return "EBADF: The argument sockfd is an invalid file descriptor.";
        case ECONNREFUSED:
            return "ECONNREFUSED: A remote host refused to allow the network connection (typically because it is not running the requested service).";
        case EFAULT:
            return "EFAULT: The receive buffer pointer(s) point outside the process's address space.";
        case EINTR:
            return "EINTR: The receive was interrupted by delivery of a signal before any data was available.";
        case EINVAL:
            return "EINVAL: Invalid argument passed.";
        case ENOMEM:
            return "ENOMEM: Could not allocate memory for recvmsg().";
        case ENOTCONN:
            return "ENOTCONN: The socket is associated with a connection-oriented protocol and has not been connected.";
        case ENOTSOCK:
            return "ENOTSOCK: The file descriptor sockfd does not refer to a socket.";
        default:
            return "Unavailble error";
    }
}
