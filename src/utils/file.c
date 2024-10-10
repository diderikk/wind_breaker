#include "file.h"
#include <unistd.h>
#include <stdio.h>

#define STATIC_PATH "static/"

int find_static_file(char *uri){
    char full_path[512 + sizeof(STATIC_PATH)];
    snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, uri);

    printf("Checking file: %s\n", full_path);

    if(access(full_path, F_OK) == 0){
        return 1;
    }

    return 0;
}

int read_static_file(char *uri, char *buffer, size_t buffer_size){
    char full_path[512 + sizeof(STATIC_PATH)];
    snprintf(full_path, sizeof(full_path), "%s%s", STATIC_PATH, uri);

    FILE *file = fopen(full_path, "r");
    if(file == NULL){
        return -1;
    }

    size_t read_size = fread(buffer, 1, buffer_size, file);
    fclose(file);

    return read_size;
}
