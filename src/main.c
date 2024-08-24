#include "socket.h"
#include "signal.h"
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include <unistd.h>

#define PORT "8080"
#define LISTEN_BACKLOG 50 // TODO: Justify
#define BUFFER_SIZE 1024

int socket_fd = -1;

void handle_signal(int signum);

int main(int argc, char *argv[]){
    struct addrinfo hints, *servinfo, *p;
    int client_socket_fd;
    struct sockaddr_storage client_addr;
    int return_val;
    int yes = 1;
    char ip_str[INET6_ADDRSTRLEN], data[BUFFER_SIZE];

    printf("Running main with %d args:\n", argc);
    for(int i = 0; i < argc; ++i) {
        printf("Argument %d: %s\n", i + 1, argv[i]);
    }
 
    // https://beej.us/guide/bgnet/html/split/client-server-background.html
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4, AF_INET6: IPv6 and AF_UNSPECT: BOTH
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; // Use for binding to any local address. 

    // https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
    // Simply put: fetches all possible hosts that the socket can be bound to
    // based on the options selected above
    if ((return_val = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
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

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Continously listen for new connections
    while(1) {
        printf("Polling for new client to connect...\n");
        client_socket_fd = accept_socket(socket_fd, (struct sockaddr *)&client_addr);


        get_in_addr_str((struct sockaddr *)&client_addr, ip_str, sizeof(ip_str));
        printf("Client connect %s:%d\n", ip_str, get_in_addr_port((struct sockaddr *)&client_addr));

        int recv_return = recv_socket(client_socket_fd, data, BUFFER_SIZE, SHOULD_NOT_EXIT);
        if(recv_return >= 0){
            printf("Client request: %s\n", data);

            if(strncmp(data, "Hello", 5) == 0){
                strcpy(data, "Hello, client");
            }

            send_socket(client_socket_fd, data, SHOULD_NOT_EXIT);
        }

        close(client_socket_fd);
    }


    return 0;
}

void handle_signal(int signum) {
    if (socket_fd != -1) {
        close(socket_fd);
        printf("\nSocket closed due to signal %d\n", signum);
    }
    exit(signum);
}
