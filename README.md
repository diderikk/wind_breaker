# HTTP server implemented in C

## TODO
* Logger with datetime and level => preferably JSON and to file and stdout.
* listen_async:
    * queue impl
    * Create a poll thread
        * On POLLIN, add the fd to the queue (before or after recv) and call `pthread_cond_signal(&data->cond)`
            * Queue must be mutex before addedig data
            * If after read, the data must also be stored and passed to worker thread
    * Create worker threads:
        ```
            shared_data_t* data = (shared_data_t*)arg;

            while (1) {
                pthread_mutex_lock(&data->mutex);

                // Wait for an event
                while (data->event_queue.empty()) {
                    pthread_cond_wait(&data->cond, &data->mutex);
                }

                // Get the event from the queue
                event_t event = data->event_queue.front();
                data->event_queue.pop();

                pthread_mutex_unlock(&data->mutex);

                // Process the event
                printf("Worker thread processing event from fd %d: %s\n", event.fd, event.data);
            }
        ```


## Resources
* [getaddrinfo](https://man7.org/linux/man-pages/man3/getaddrinfo.3.html)
* [Beej's Guide](https://beej.us/guide/bgnet/html/split/client-server-background.html)
* [GeeksForGeeks's Guide](https://www.geeksforgeeks.org/socket-programming-cc/)
