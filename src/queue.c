#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

void queue_init(queue_t* q, int capacity, sched_policy_t policy) {
    q->buffer = malloc(sizeof(request_t) * capacity);
    if (!q->buffer) {
        perror("Failed to allocate queue buffer");
        exit(EXIT_FAILURE);
    }
    q->capacity = capacity;
    q->size = 0;
    q->in = 0;
    q->out = 0;
    q->policy = policy;

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

void queue_push(queue_t* q, request_t request) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    q->buffer[q->in] = request;
    q->in = (q->in + 1) % q->capacity;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

request_t queue_pop(queue_t* q) {
    pthread_mutex_lock(&q->mutex);

    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    int idx_to_remove = q->out;

    if (q->policy == QUEUE_SFF) {
        int best_idx = -1;
        // Search for the request with smallest file size (and smallest seq)
        for (int i = 0; i < q->size; i++) {
            int curr_idx = (q->out + i) % q->capacity;
            if (best_idx == -1) {
                best_idx = curr_idx;
                continue;
            }

            request_t *best = &q->buffer[best_idx];
            request_t *curr = &q->buffer[curr_idx];

            if (curr->file_size < best->file_size) {
                best_idx = curr_idx;
            } else if (curr->file_size == best->file_size) {
                if (curr->seq < best->seq) {
                    best_idx = curr_idx;
                }
            }
        }

        // If the best is not at the head, swap it with the head
        if (best_idx != q->out) {
            request_t temp = q->buffer[q->out];
            q->buffer[q->out] = q->buffer[best_idx];
            q->buffer[best_idx] = temp;
        }
        idx_to_remove = q->out;
    }

    request_t request = q->buffer[idx_to_remove];
    q->out = (q->out + 1) % q->capacity;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return request;
}
