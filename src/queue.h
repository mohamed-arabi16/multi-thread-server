#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct {
    int *buffer;
    int capacity;
    int size;
    int in;
    int out;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} queue_t;

void queue_init(queue_t* q, int capacity);
void queue_destroy(queue_t* q);
void queue_push(queue_t* q, int client_fd);
int queue_pop(queue_t* q);

#endif
