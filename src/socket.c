#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h> 
#include <errno.h>
#include <unistd.h>

#pragma once

const char* handle_socket_error(int code);

int open_socket(){

    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_fd == -1) {
        perror(handle_socket_error(errno));
        close(socket_fd);
    }

    return socket_fd;
}


int bind_socket(int socket_fd){
    struct sockaddr_in addr;

    memset(


    return 0;
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
