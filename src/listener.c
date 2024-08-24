#define _GNU_SOURCE
#include "socket.h"
#include "listener.h"
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define LISTEN_BACKLOG 50 // TODO: Justify

void add_to_pfds_sync(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds_sync(struct pollfd pfds[], int i, int *fd_count);
const char* get_poll_event_description(short event);
const char* get_poll_error_description(int code);

int get_listener_socket(char* port){
    struct addrinfo hints, *servinfo, *p;
    int socket_fd, return_val;
    int yes = 1;
    char ip_str[INET6_ADDRSTRLEN];
    // https://beej.us/guide/bgnet/html/split/client-server-background.html
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4, AF_INET6: IPv6 and AF_UNSPECT: BOTH
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; // Use for binding to any local address. 

    // https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
    // Simply put: fetches all possible hosts that the socket can be bound to
    // based on the options selected above
    if ((return_val = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(return_val));
        return EXIT_FAILURE;
    }

    
    // Iterates through all possible hosts and tries to bind a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((socket_fd = open_socket(p)) < 0)
            continue;

        // allows the socket to bind to an address that is in a TIME_WAIT state.
        // Allows for quick server restarts
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("Could not bind to the same address\n");
            exit(EXIT_FAILURE);
        }

        if((bind_socket(socket_fd, p)) < 0)
            continue;

        break;
    }

    freeaddrinfo(servinfo);

    if(p == NULL){
        perror("Failed to bind the socket\n");
        exit(EXIT_FAILURE);
    }
 
    // Begins listening, BACKLOG is the max amount of waiting connections
    listen_socket(socket_fd, LISTEN_BACKLOG);


    
    get_in_addr_str(p->ai_addr, ip_str, sizeof(ip_str));
    printf("Listening for connections on: Address: %s, Port %d\n", ip_str, get_in_addr_port(p->ai_addr));

    return socket_fd;
}



void listen_sync(int listener, size_t buffer_size){
    struct sockaddr_storage client_addr;
    int client_socket_fd;
    char ip_str[INET6_ADDRSTRLEN], data[buffer_size];
    sigset_t sigmask;
    sigemptyset(&sigmask);
    int fd_count = 0;
    int poll_array_size = 5;
    struct pollfd *poll_array = malloc(sizeof *poll_array * poll_array_size);

    poll_array[0].fd = listener;
    poll_array[0].events = POLLIN; // Report ready to read on incoming connection

    fd_count = 1;

    /* 
     * TODO: Create reset function that is executed on relevant errors
     * => Function must free or reallocate poll_array
     * Check for error first => handle error first ^
     *
     * Handle other revents types to prevent clustering of bad fds.
     * Maybe validate fds on every loop of poll_array.
     */

    // Continously listen for new connections
    while(1) {
        // https://man7.org/linux/man-pages/man2/poll.2.html
        // NULL causes the poll system call to poll until a revents 
        // is updated by the kernel
        int poll_count = ppoll(poll_array, fd_count, NULL, &sigmask);

        if(poll_count < 0){
            perror(get_poll_error_description(errno));
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < fd_count; i++){
            
        }

        printf("Polling for new client to connect...\n");
        client_socket_fd = accept_socket(listener, (struct sockaddr *)&client_addr);


        get_in_addr_str((struct sockaddr *)&client_addr, ip_str, sizeof(ip_str));
        printf("Client connect %s:%d\n", ip_str, get_in_addr_port((struct sockaddr *)&client_addr));

        int recv_return = recv_socket(client_socket_fd, data, buffer_size, SHOULD_NOT_EXIT);
        if(recv_return >= 0){
            printf("Client request: %s\n", data);

            if(strncmp(data, "Hello", 5) == 0){
                strcpy(data, "Hello, client");
            }

            send_socket(client_socket_fd, data, SHOULD_NOT_EXIT);
        }

        close(client_socket_fd);
    }
    //NEVER RETURNS
}



// https://beej.us/guide/bgnet/html/split/slightly-advanced-techniques.html
// Add a new file descriptor to the set
void add_to_pfds_sync(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds_sync(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

const char* get_poll_event_description(short event) {
    switch (event) {
        case POLLIN:
            return "There is data to read.";
        case POLLPRI:
            return "There is some exceptional condition on the file descriptor.";
        case POLLOUT:
            return "Writing is now possible, though a write larger than the available space in a socket or pipe will still block (unless O_NONBLOCK is set).";
        case POLLRDHUP:
            return "Stream socket peer closed connection, or shut down writing half of connection.";
        case POLLERR:
            return "Error condition (only returned in revents; ignored in events).";
        case POLLHUP:
            return "Hang up (only returned in revents; ignored in events).";
        case POLLNVAL:
            return "Invalid request: fd not open (only returned in revents; ignored in events).";
        default:
            return "Unknown event.";
    }
}

const char* get_poll_error_description(int code) {
    switch (code) {
        case EFAULT:
            return "fds points outside the process's accessible address space. The array given as argument was not contained in the calling program's address space.";
        case EINTR:
            return "A signal occurred before any requested event; see signal(7).";
        case EINVAL:
            return "The nfds value exceeds the RLIMIT_NOFILE value.";
        case ENOMEM:
            return "Unable to allocate memory for kernel data structures.";
        default:
            return "Unknown error.";
    }
}
