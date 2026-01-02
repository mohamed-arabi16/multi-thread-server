#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "queue.h"

#define PORT 8080
#define NUM_THREADS 4
#define DEFAULT_QUEUE_SIZE 16
#define BACKLOG 10
#define REQUEST_BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 8192

// Macros for stringification to ensure format string matches buffer size
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define PATH_MAX_LEN 1023
#define PATH_BUF_SIZE (PATH_MAX_LEN + 1)
#define PATH_FMT "%" STR(PATH_MAX_LEN) "s"

queue_t q;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
int request_seq = 0;
sched_policy_t scheduling_policy = QUEUE_FIFO;
int num_threads = NUM_THREADS;
int queue_size = DEFAULT_QUEUE_SIZE;

static void send_response(request_t *req);
static void send_404(int client_fd);
static const char *get_mime_type(const char *path);
static int send_all(int fd, const void *buf, size_t len);
static void *worker_thread(void *arg);
static void parse_request(int client_fd, char *path_out);
static void log_request_arrival(request_t *req);
static void log_worker_pickup(int worker_id, request_t *req);

int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "fifo") == 0) {
                scheduling_policy = QUEUE_FIFO;
            } else if (strcmp(argv[i+1], "sff") == 0) {
                scheduling_policy = QUEUE_SFF;
            } else {
                fprintf(stderr, "Unknown policy: %s. Using default.\n", argv[i+1]);
            }
            i++;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
             num_threads = atoi(argv[i+1]);
             if (num_threads < 1) num_threads = 1;
             i++;
        } else if ((strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--queue-size") == 0) && i + 1 < argc) {
             queue_size = atoi(argv[i+1]);
             if (queue_size < 1) queue_size = 1;
             i++;
        }
    }

    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d with policy %s with %d threads and queue size %d\n",
           PORT, scheduling_policy == QUEUE_FIFO ? "FIFO" : "SFF", num_threads, queue_size);
    fflush(stdout);

    queue_init(&q, queue_size, scheduling_policy);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    int *thread_ids = malloc(sizeof(int) * num_threads);

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_ids[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        request_t req;
        req.client_fd = client_fd;

        // Parse request to get path
        parse_request(client_fd, req.path);

        // Stat file
        const char *file_path = req.path;
        if (file_path[0] == '/') file_path++;
        if (file_path[0] == '\0') file_path = "index.html";

        struct stat st;
        if (stat(file_path, &st) < 0 || !S_ISREG(st.st_mode)) {
             req.file_size = -1; // Missing or invalid
        } else {
             req.file_size = st.st_size;
        }

        // Timestamp and Seq
        gettimeofday(&req.arrival_time, NULL);

        pthread_mutex_lock(&log_mutex);
        req.seq = request_seq++;
        log_request_arrival(&req);
        pthread_mutex_unlock(&log_mutex);

        queue_push(&q, req);
    }

    free(threads);
    free(thread_ids);
    close(server_fd);
    return 0;
}

static void *worker_thread(void *arg) {
    int thread_id = *(int *)arg;
    while (1) {
        request_t req = queue_pop(&q);

        pthread_mutex_lock(&log_mutex);
        log_worker_pickup(thread_id, &req);
        pthread_mutex_unlock(&log_mutex);

        send_response(&req);
        close(req.client_fd);
    }
    return NULL;
}

static void parse_request(int client_fd, char *path_out) {
    char buffer[REQUEST_BUFFER_SIZE];
    size_t total_read = 0;

    // Initialize path to empty in case of failure
    path_out[0] = '\0';

    // We peek first or just read.
    // Spec says "Producer ... reads and parses".
    // We need to read enough to get the request line.
    // NOTE: This simple implementation might consume part of the body or subsequent requests,
    // but for this assignment, we assume simple GET requests.
    // Also, we must be careful not to block forever if client sends nothing.
    // But assuming valid clients for now.

    while (total_read < sizeof(buffer) - 1) {
        ssize_t bytes_read = recv(client_fd, buffer + total_read, sizeof(buffer) - 1 - total_read, MSG_PEEK);
        if (bytes_read <= 0) break; // Error or closed

        // Check if we have a full line
        char *newline = memchr(buffer + total_read, '\n', bytes_read);
        if (newline) {
             // We found a newline. Now perform the actual read up to the end of headers or at least the first line.
             // Actually, to keep it simple and consistent with previous handle_client:
             // We read everything available or until \r\n\r\n.
             // But wait, if we use MSG_PEEK, we haven't consumed it.
             // We should consume it because the worker won't read it.

             // Let's just read.
             bytes_read = read(client_fd, buffer + total_read, sizeof(buffer) - 1 - total_read);
             if (bytes_read <= 0) break;
             total_read += bytes_read;
             buffer[total_read] = '\0';

             if (strstr(buffer, "\r\n\r\n")) break;
        } else {
             // Just read more
             bytes_read = read(client_fd, buffer + total_read, sizeof(buffer) - 1 - total_read);
             if (bytes_read <= 0) break;
             total_read += bytes_read;
             buffer[total_read] = '\0';
        }
    }

    // Parse method and path
    char method[16];
    char path[PATH_BUF_SIZE];
    if (sscanf(buffer, "%15s " PATH_FMT, method, path) == 2) {
        if (strcmp(method, "GET") == 0) {
            strncpy(path_out, path, PATH_MAX_LEN);
            path_out[PATH_MAX_LEN] = '\0';
        }
    }
}

static void send_response(request_t *req) {
    int client_fd = req->client_fd;
    const char *path = req->path;

    if (path[0] == '\0') {
        // Invalid request or parse failure
        // We could send 400, but let's just close or send 404
        return;
    }

    const char *file_path = path;
    if (path[0] == '/') {
        file_path = path + 1;
        if (file_path[0] == '\0') {
            file_path = "index.html";
        }
    }

    // We already stat-ed in the producer, but we need to open it here.
    // We can rely on req->file_size to know if it's missing (-1).
    if (req->file_size == -1) {
        send_404(client_fd);
        return;
    }

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        send_404(client_fd);
        return;
    }

    const char *content_type = get_mime_type(file_path);

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.0 200 OK\r\n"
                              "Content-Length: %ld\r\n"
                              "Content-Type: %s\r\n"
                              "\r\n",
                              (long)req->file_size, content_type);

    if (header_len < 0 || header_len >= (int)sizeof(header)) {
        close(fd);
        return;
    }

    if (send_all(client_fd, header, (size_t)header_len) < 0) {
        close(fd);
        return;
    }

    char buffer[FILE_BUFFER_SIZE];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        if (send_all(client_fd, buffer, (size_t)n) < 0) {
            break;
        }
    }

    close(fd);
}

static void send_404(int client_fd) {
    const char *body = "404 Not Found\n";
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.0 404 Not Found\r\n"
                              "Content-Length: %zu\r\n"
                              "Content-Type: text/plain\r\n"
                              "\r\n",
                              strlen(body));

    if (header_len > 0 && header_len < (int)sizeof(header)) {
        send_all(client_fd, header, (size_t)header_len);
    }
    send_all(client_fd, body, strlen(body));
}

static const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext != NULL) {
        if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
            return "text/html";
        }
        if (strcmp(ext, ".txt") == 0) {
            return "text/plain";
        }
    }
    return "application/octet-stream";
}

static int send_all(int fd, const void *buf, size_t len) {
    const char *ptr = (const char *)buf;
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t sent = write(fd, ptr + total_sent, len - total_sent);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (sent == 0) {
            return -1;
        }
        total_sent += (size_t)sent;
    }

    return 0;
}

static void log_request_arrival(request_t *req) {
    struct tm *tm_info;
    char time_buffer[30];
    time_t tv_sec = req->arrival_time.tv_sec;
    int millis = req->arrival_time.tv_usec / 1000;

    tm_info = localtime(&tv_sec);
    strftime(time_buffer, 20, "%Y-%m-%dT%H:%M:%S", tm_info);

    printf("REQUEST seq=%d path=\"%s\" time=%s.%03d\n",
           req->seq, req->path, time_buffer, millis);
    fflush(stdout);
}

static void log_worker_pickup(int worker_id, request_t *req) {
    printf("WORKER %d picked request with seq=%d size=%ld\n",
           worker_id, req->seq, (long)req->file_size);
    fflush(stdout);
}
