#include "./socket.c"
#include "signal.h"
#include <stdio.h>

#define PORT 8080
#define LISTEN_BACKLOG 50 // TODO: Justify

int socket_fd = -1;

void handle_signal(int signum);

int main(int argc, char *argv[]){
    printf("Running main with %d args:\n", argc);
    for(int i = 0; i < argc; ++i) {
        printf("Argument %d: %s\n", i + 1, argv[i]);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    socket_fd = open_socket(); 
 
    printf("Opened socket with fd: %d\n", socket_fd);

    int bind_result = bind_socket(socket_fd, PORT);

    printf("Bound socket with result: %d\n", bind_result);

    int listen_result = listen_socket(socket_fd, LISTEN_BACKLOG);

    printf("Listening to socket with result: %d\n", listen_result);

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));

    printf("Polling for client to connect...\n");
    int client_socket_fd = accept_socket(socket_fd, &client_addr);

    printf("Client connect with socket_fd: %d\n", client_socket_fd);

    close(socket_fd);
    

    return 0;
}

void handle_signal(int signum) {
    if (socket_fd != -1) {
        close(socket_fd);
        printf("Socket closed due to signal %d\n", signum);
    }
    exit(signum);
}
