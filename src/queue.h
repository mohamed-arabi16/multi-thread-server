#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>

typedef struct {
    int client_fd;
    int seq;
    struct timeval arrival_time;
    char path[1024];
    off_t file_size;
} request_t;

typedef enum {
    QUEUE_FIFO,
    QUEUE_SFF
} sched_policy_t;

typedef struct {
    request_t *buffer;
    int capacity;
    int size;
    int in;
    int out;
    sched_policy_t policy;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} queue_t;

void queue_init(queue_t* q, int capacity, sched_policy_t policy);
void queue_destroy(queue_t* q);
void queue_push(queue_t* q, request_t request);
request_t queue_pop(queue_t* q);

#endif
