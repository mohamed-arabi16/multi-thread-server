#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

void queue_init(queue_t* q, int capacity) {
    q->buffer = malloc(sizeof(int) * capacity);
    if (!q->buffer) {
        perror("Failed to allocate queue buffer");
        exit(EXIT_FAILURE);
    }
    q->capacity = capacity;
    q->size = 0;
    q->in = 0;
    q->out = 0;

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(q->buffer);
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&q->not_empty, NULL) != 0) {
        perror("Failed to initialize not_empty cond");
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&q->not_full, NULL) != 0) {
        perror("Failed to initialize not_full cond");
        pthread_cond_destroy(&q->not_empty);
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        exit(EXIT_FAILURE);
    }
}

void queue_destroy(queue_t* q) {
    if (q->buffer) {
        free(q->buffer);
    }
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

void queue_push(queue_t* q, int client_fd) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    q->buffer[q->in] = client_fd;
    q->in = (q->in + 1) % q->capacity;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

int queue_pop(queue_t* q) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    int client_fd = q->buffer[q->out];
    q->out = (q->out + 1) % q->capacity;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return client_fd;
}
