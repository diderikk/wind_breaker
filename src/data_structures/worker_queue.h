#ifndef QUEUE_H
#define QUEUE_H

#include "../static.h"

int init_queue();
void destroy_queue();
void broadcast_queue();
void queue_push(int fd, const char *data, int size);
void set_work_ready(int fd);
worker_data *queue_pop();

#endif // QUEUE_H
