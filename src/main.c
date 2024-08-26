#include "listener.h"
#include "signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PORT "8080"
#define BUFFER_SIZE 1024

int socket_fd = -1;

void handle_signal(int signum);

int main(int argc, char *argv[]){
    printf("Running main with %d args:\n", argc);
    for(int i = 0; i < argc; ++i) {
        printf("Argument %d: %s\n", i + 1, argv[i]);
    }

    socket_fd = get_listener_socket(PORT);

    if(socket_fd < 0){
        perror("Failed to initalize socket");
        exit(EXIT_FAILURE);
    }
 
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    listen_sync(socket_fd, BUFFER_SIZE);

    return 0;
}


void handle_signal(int signum) {
    if (socket_fd != -1) {
        close(socket_fd);
        printf("\nSocket closed due to signal %d\n", signum);
    }
    exit(signum);
}
