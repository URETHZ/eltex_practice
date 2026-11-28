#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        9090
#define MAX_CLIENTS 64
#define BUF_SIZE    65536
#define MAX_NICK    32
#define MAX_EVENTS  (MAX_CLIENTS + 1)

typedef struct {
    int    fd;
    int    active;
    char   nick[MAX_NICK];

    char   buf[BUF_SIZE];
    size_t buf_len;

    /* состояние конечного автомата приёма файла */
    int    state;               /* 0 = ждём заголовок, 1 = читаем тело файла */
    char   file_name[256];
    size_t file_remain;         /* сколько байт тела ещё принять */
} client_t;

static client_t clients[MAX_CLIENTS];
static int      epoll_fd;
static int      listen_fd;

/* ---------- утилиты ---------- */

static int find_client_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && clients[i].fd == fd) return i;
    return -1;
}

static int alloc_client(void) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (!clients[i].active) return i;
    return -1;
}

static void broadcast_raw(int exclude_fd, const void *data, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active || clients[i].fd == exclude_fd) continue;
        const char *p = data;
        size_t left = len;
        while (left > 0) {
            ssize_t r = send(clients[i].fd, p, left, MSG_NOSIGNAL);
            if (r <= 0) break;
            p += r; left -= r;
        }
    }
}

static void broadcast_text(int exclude_fd, const char *text) {
    broadcast_raw(exclude_fd, text, strlen(text));
}

static void remove_client(int idx) {
    client_t *c = &clients[idx];
    if (!c->active) return;

    char msg[256];
    snprintf(msg, sizeof(msg), "LEAVE|%s\n", c->nick);
    broadcast_text(c->fd, msg);

    printf("[server] client left: %s (fd=%d)\n", c->nick, c->fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    memset(c, 0, sizeof(*c));
}

/* ---------- парсер протокола ---------- */

static int process_client(int idx) {
    client_t *c = &clients[idx];

    while (1) {
        if (c->state == 0) {
            /* ищем конец заголовка */
            char *nl = memchr(c->buf, '\n', c->buf_len);
            if (!nl) return 0;           /* ждём ещё данных */

            size_t hdr_len = nl - c->buf;
            if (hdr_len >= 4096) {       /* защита от мусора */
                memmove(c->buf, nl + 1, c->buf_len - hdr_len - 1);
                c->buf_len -= hdr_len + 1;
                continue;
            }

            char header[4096];
            memcpy(header, c->buf, hdr_len);
            header[hdr_len] = '\0';

            size_t shift = hdr_len + 1;
            memmove(c->buf, c->buf + shift, c->buf_len - shift);
            c->buf_len -= shift;

            char *saveptr = NULL;
            char *cmd  = strtok_r(header, "|", &saveptr);
            if (!cmd) continue;

            if (strcmp(cmd, "JOIN") == 0) {
                char *nick = strtok_r(NULL, "|", &saveptr);
                if (nick) {
                    strncpy(c->nick, nick, MAX_NICK - 1);
                    c->nick[MAX_NICK - 1] = '\0';
                    printf("[server] client joined: %s (fd=%d)\n", c->nick, c->fd);
                    char out[256];
                    snprintf(out, sizeof(out), "JOIN|%s\n", c->nick);
                    broadcast_text(c->fd, out);
                }
            } else if (strcmp(cmd, "LEAVE") == 0) {
                remove_client(idx);
                return -1;
            } else if (strcmp(cmd, "MSG") == 0) {
                char *nick = strtok_r(NULL, "|", &saveptr);
                char *text = saveptr;           /* всё, что осталось */
                if (!nick) continue;
                char out[BUF_SIZE];
                int n = snprintf(out, sizeof(out), "MSG|%s|%s\n",
                                 nick, text ? text : "");
                broadcast_raw(c->fd, out, n);
            } else if (strcmp(cmd, "FILE") == 0) {
                char *nick  = strtok_r(NULL, "|", &saveptr);
                char *fname = strtok_r(NULL, "|", &saveptr);
                char *sz    = strtok_r(NULL, "|", &saveptr);
                if (!nick || !fname || !sz) continue;

                strncpy(c->file_name, fname, sizeof(c->file_name) - 1);
                c->file_remain = (size_t)atol(sz);
                c->state = 1;

                char out[4096];
                int n = snprintf(out, sizeof(out), "FILE|%s|%s|%zu\n",
                                 nick, fname, c->file_remain);
                broadcast_raw(c->fd, out, n);
                /* если в буфере уже есть часть тела — обработаем ниже */
            }
        } else { /* state == 1 — приём тела файла */
            if (c->buf_len == 0) return 0;

            size_t chunk = c->buf_len < c->file_remain
                           ? c->buf_len : c->file_remain;
            broadcast_raw(c->fd, c->buf, chunk);

            c->file_remain -= chunk;
            memmove(c->buf, c->buf + chunk, c->buf_len - chunk);
            c->buf_len -= chunk;

            if (c->file_remain == 0) {
                c->state = 0;
                printf("[server] file '%s' from %s relayed\n",
                       c->file_name, c->nick);
            }
        }
    }
}

/* ---------- main ---------- */

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(listen_fd, 16) < 0) {
        perror("listen"); return 1;
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); return 1; }

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    memset(clients, 0, sizeof(clients));

    printf("[server] listening on port %d (multiplexing: epoll)\n", PORT);

    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                struct sockaddr_in cli_addr;
                socklen_t len = sizeof(cli_addr);
                int cfd = accept(listen_fd, (struct sockaddr*)&cli_addr, &len);
                if (cfd < 0) { perror("accept"); continue; }

                int idx = alloc_client();
                if (idx < 0) {
                    fprintf(stderr, "[server] max clients reached\n");
                    close(cfd);
                    continue;
                }
                memset(&clients[idx], 0, sizeof(client_t));
                clients[idx].fd     = cfd;
                clients[idx].active = 1;
                snprintf(clients[idx].nick, MAX_NICK, "anon-%d", cfd);

                ev.events  = EPOLLIN;
                ev.data.fd = cfd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cfd, &ev);
                printf("[server] new connection fd=%d from %s:%d\n",
                       cfd, inet_ntoa(cli_addr.sin_addr),
                       ntohs(cli_addr.sin_port));
            } else {
                int idx = find_client_by_fd(fd);
                if (idx < 0) continue;

                client_t *c = &clients[idx];
                size_t space = sizeof(c->buf) - c->buf_len;
                if (space == 0) {       /* буфер переполнен — отключаем */
                    remove_client(idx);
                    continue;
                }

                ssize_t r = recv(fd, c->buf + c->buf_len, space, 0);
                if (r <= 0) {
                    remove_client(idx);
                    continue;
                }
                c->buf_len += r;
                if (process_client(idx) < 0) continue;
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}