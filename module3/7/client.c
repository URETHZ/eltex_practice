#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT       9090
#define BUF_SIZE   65536
#define MAX_NICK   32
#define RECV_DIR   "received"

static int    sock_fd;
static char   my_nick[MAX_NICK];

/* буфер приёма от сервера */
static char   recv_buf[BUF_SIZE];
static size_t recv_buf_len = 0;

/* состояние приёма файла */
static int    recv_state = 0;           /* 0 = заголовок, 1 = тело */
static char   recv_file_name[256];
static char   recv_file_sender[MAX_NICK];
static size_t recv_file_remain = 0;
static FILE  *recv_file_fp = NULL;

/* ---------- утилиты ---------- */

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t r = send(fd, p, len, MSG_NOSIGNAL);
        if (r <= 0) return -1;
        p += r; len -= r;
    }
    return 0;
}

/* ---------- обработка данных от сервера ---------- */

static void process_server_data(void) {
    while (1) {
        if (recv_state == 0) {
            char *nl = memchr(recv_buf, '\n', recv_buf_len);
            if (!nl) return;

            size_t hdr_len = nl - recv_buf;
            if (hdr_len >= 4096) {
                memmove(recv_buf, nl + 1, recv_buf_len - hdr_len - 1);
                recv_buf_len -= hdr_len + 1;
                continue;
            }

            char header[4096];
            memcpy(header, recv_buf, hdr_len);
            header[hdr_len] = '\0';

            size_t shift = hdr_len + 1;
            memmove(recv_buf, recv_buf + shift, recv_buf_len - shift);
            recv_buf_len -= shift;

            char *saveptr = NULL;
            char *cmd = strtok_r(header, "|", &saveptr);
            if (!cmd) continue;

            if (strcmp(cmd, "MSG") == 0) {
                char *nick = strtok_r(NULL, "|", &saveptr);
                char *text = saveptr;
                printf("\n[%s]: %s\n> ", nick ? nick : "?", text ? text : "");
                fflush(stdout);
            } else if (strcmp(cmd, "JOIN") == 0) {
                char *nick = strtok_r(NULL, "|", &saveptr);
                printf("\n*** %s joined ***\n> ", nick ? nick : "?");
                fflush(stdout);
            } else if (strcmp(cmd, "LEAVE") == 0) {
                char *nick = strtok_r(NULL, "|", &saveptr);
                printf("\n*** %s left ***\n> ", nick ? nick : "?");
                fflush(stdout);
            } else if (strcmp(cmd, "FILE") == 0) {
                char *nick  = strtok_r(NULL, "|", &saveptr);
                char *fname = strtok_r(NULL, "|", &saveptr);
                char *sz    = strtok_r(NULL, "|", &saveptr);
                if (!nick || !fname || !sz) continue;

                strncpy(recv_file_sender, nick, MAX_NICK - 1);
                strncpy(recv_file_name, fname, sizeof(recv_file_name) - 1);
                recv_file_remain = (size_t)atol(sz);
                recv_state = 1;

                char path[512];
                snprintf(path, sizeof(path), "%s/%s_from_%s",
                         RECV_DIR, fname, nick);
                recv_file_fp = fopen(path, "wb");
                if (!recv_file_fp) {
                    perror("fopen");
                    recv_state = 0;
                    continue;
                }
                printf("\n[receiving '%s' (%zu bytes) from %s -> %s]\n> ",
                       fname, recv_file_remain, nick, path);
                fflush(stdout);
            }
        } else { /* приём тела файла */
            if (recv_buf_len == 0) return;

            size_t chunk = recv_buf_len < recv_file_remain
                           ? recv_buf_len : recv_file_remain;
            if (recv_file_fp)
                fwrite(recv_buf, 1, chunk, recv_file_fp);

            recv_file_remain -= chunk;
            memmove(recv_buf, recv_buf + chunk, recv_buf_len - chunk);
            recv_buf_len -= chunk;

            if (recv_file_remain == 0) {
                if (recv_file_fp) { fclose(recv_file_fp); recv_file_fp = NULL; }
                printf("\n[file '%s' from %s saved]\n> ",
                       recv_file_name, recv_file_sender);
                fflush(stdout);
                recv_state = 0;
            }
        }
    }
}

/* ---------- отправка файла ---------- */

static int send_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return -1; }

    fseek(fp, 0, SEEK_END);
    size_t sz = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    char header[4096];
    int hlen = snprintf(header, sizeof(header),
                        "FILE|%s|%s|%zu\n", my_nick, base, sz);
    if (send_all(sock_fd, header, hlen) < 0) { fclose(fp); return -1; }

    char buf[65536];
    size_t total = 0;
    while (total < sz) {
        size_t r = fread(buf, 1, sizeof(buf), fp);
        if (r == 0) break;
        if (send_all(sock_fd, buf, r) < 0) { fclose(fp); return -1; }
        total += r;
    }
    fclose(fp);
    return 0;
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_ip> <nickname>\n", argv[0]);
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    const char *server_ip = argv[1];
    snprintf(my_nick, sizeof(my_nick), "%s", argv[2]);

    mkdir(RECV_DIR, 0755);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid server ip\n");
        return 1;
    }
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    char join[256];
    snprintf(join, sizeof(join), "JOIN|%s\n", my_nick);
    send_all(sock_fd, join, strlen(join));

    printf("Connected to %s as '%s' (multiplexing: select)\n",
           server_ip, my_nick);
    printf("Commands:\n");
    printf("  <text>         — send message to all\n");
    printf("  /file <path>   — send file to all\n");
    printf("  /quit          — exit\n");
    printf("> ");
    fflush(stdout);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sock_fd, &rfds);
        int maxfd = STDIN_FILENO > sock_fd ? STDIN_FILENO : sock_fd;

        int r = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(sock_fd, &rfds)) {
            size_t space = sizeof(recv_buf) - recv_buf_len;
            if (space == 0) {
                fprintf(stderr, "\n[recv buffer overflow]\n");
                break;
            }
            ssize_t n = recv(sock_fd, recv_buf + recv_buf_len, space, 0);
            if (n <= 0) {
                printf("\n[server disconnected]\n");
                break;
            }
            recv_buf_len += n;
            process_server_data();
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char line[4096];
            if (!fgets(line, sizeof(line), stdin)) break;

            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
            if (len == 0) {
                printf("> "); fflush(stdout);
                continue;
            }

            if (strcmp(line, "/quit") == 0) {
                char lv[256];
                snprintf(lv, sizeof(lv), "LEAVE|%s\n", my_nick);
                send_all(sock_fd, lv, strlen(lv));
                break;
            } else if (strncmp(line, "/file ", 6) == 0) {
                if (send_file(line + 6) == 0)
                    printf("[file sent]\n");
                else
                    printf("[file send failed]\n");
            } else {
                char msg[BUF_SIZE];
                int mlen = snprintf(msg, sizeof(msg),
                                    "MSG|%s|%s\n", my_nick, line);
                if (send_all(sock_fd, msg, mlen) < 0) {
                    printf("[send error]\n");
                    break;
                }
            }
            printf("> ");
            fflush(stdout);
        }
    }

    if (recv_file_fp) { fclose(recv_file_fp); recv_file_fp = NULL; }
    close(sock_fd);
    printf("\nBye.\n");
    return 0;
}