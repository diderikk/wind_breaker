#include <stdio.h>

int main(int argc, char *argv[]){
    printf("Running main with %d args:\n", argc);
    for(int i = 0; i < argc; ++i) {
        printf("Argument %d: %s\n", i + 1, argv[i]);
    }

    return 0;
}
