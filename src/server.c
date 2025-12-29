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
#include <unistd.h>

#define PORT 8080
#define BACKLOG 10
#define REQUEST_BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 8192

static void handle_client(int client_fd);
static void send_404(int client_fd);
static const char *get_mime_type(const char *path);
static int send_all(int fd, const void *buf, size_t len);

int main(void) {
    int server_fd;
    struct sockaddr_in server_addr;

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

    printf("Server listening on port %d\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}

static void handle_client(int client_fd) {
    char request[REQUEST_BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, request, sizeof(request) - 1);

    if (bytes_read <= 0) {
        return;
    }

    request[bytes_read] = '\0';

    char method[8];
    char path[PATH_MAX];

    if (sscanf(request, "%7s %1023s", method, path) != 2) {
        return;
    }

    if (strcmp(method, "GET") != 0) {
        return;
    }

    const char *file_path = path;
    if (path[0] == '/') {
        file_path = path + 1;
        if (file_path[0] == '\0') {
            file_path = "index.html";
        }
    }

    struct stat st;
    if (stat(file_path, &st) < 0 || !S_ISREG(st.st_mode)) {
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
                              (long)st.st_size, content_type);

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
        total_sent += (size_t)sent;
    }

    return 0;
}
