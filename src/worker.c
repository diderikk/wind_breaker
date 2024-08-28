#include "worker.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


pthread_t * initialize_workers(int requested_size){
    
    pthread_t *workers = malloc(sizeof *workers * requested_size);

    return workers;
}


int close_workers(pthread_t *threads[], int *thread_count){
    int closed_threads = 0;
    for(int i = 0; i < *thread_count; i++){
        // Request thread cancellation
    //    if (pthread_cancel(*threads[i]) != 0) {
    //        perror("Failed to cancel thread");
    //        return -1;
    //    }

        // Wait for the thread to exit
        if (pthread_join(*threads[i], NULL) != 0) {
            perror("Failed to join thread");
            return -1;
        }
    }
    return closed_threads;
}

void add_to_threads_sync(pthread_t *threads[], pthread_t new_pthread, int *thread_count, int *thread_size)
{
    if (*thread_count == *thread_size) {
        *thread_size *= 2; // Double it

        *threads = realloc(*threads, sizeof(**threads) * (*thread_size));
    }

    (*threads)[*thread_count] = new_pthread;

    (*thread_count)++;
}

// Remove an index from the set
void del_from_threads_sync(pthread_t threads[], int* i, int *thread_count)
{
    // Copy the one from the end over this one
    threads[*i] = threads[*thread_count-1];

    (*thread_count)--;
    (*i)--;
}
