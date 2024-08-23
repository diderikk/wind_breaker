#include <asm-generic/errno-base.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <errno.h>
#include <unistd.h>
#include "string.h"

#pragma once

const char* handle_socket_error(int code);
const char* handle_bind_error(int code);
const char* handle_listen_error(int code);
const char* handle_accept_error(int code);

int open_socket(){

    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_fd == -1) {
        perror(handle_socket_error(errno));
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    return socket_fd;
}


int bind_socket(int socket_fd, short port){
    int bind_result;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind_result = bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr));

    if(bind_result < 0){
        perror(handle_bind_error(errno));
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    return bind_result;
}

int listen_socket(int socket_fd, int backlog_size){
    int listen_result;

    listen_result = listen(socket_fd, backlog_size);

    if(listen_result < 0){
        perror(handle_listen_error(errno));
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    return listen_result;
}

int accept_socket(int socket_fd, struct sockaddr_in * in_addr){
    int client_socket_fd;
    socklen_t in_addr_size;

    in_addr_size = sizeof(*in_addr);
    client_socket_fd = accept(socket_fd, (struct sockaddr*)in_addr, &in_addr_size);
    
    if(client_socket_fd < 0){
        perror(handle_accept_error(errno));
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
   
    return client_socket_fd;
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
