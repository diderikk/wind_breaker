#include "./socket.c"
#include <stdio.h>

int main(int argc, char *argv[]){
    printf("Running main with %d args:\n", argc);
    for(int i = 0; i < argc; ++i) {
        printf("Argument %d: %s\n", i + 1, argv[i]);
    }

   int socket_fd = open_socket(); 
   
   printf("Opened socket with fd: %d\n", socket_fd);
   close(socket_fd);
    

    return 0;
}
