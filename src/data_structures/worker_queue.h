#ifndef QUEUE_H
#define QUEUE_H

#define QUEUE_MAX_SIZE 1024

#include "pthread.h"

typedef struct {
  void *data[QUEUE_MAX_SIZE];
  int front;
  int rear;
  int count;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} queue_t;

int init_queue();
int destory_queue();
void queue_push(void *data);
void *queue_pop();

#endif // QUEUE_H
