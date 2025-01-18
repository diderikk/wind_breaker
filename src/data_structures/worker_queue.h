#ifndef QUEUE_H
#define QUEUE_H

#include "../static.h"
#include "pthread.h"

typedef struct {
  worker_data **data;
  int front;
  int rear;
  int count;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} queue_t;

int init_queue();
void destroy_queue();
void broadcast_queue();
void queue_push(int fd, const char *data);
worker_data *queue_pop();

#endif // QUEUE_H
