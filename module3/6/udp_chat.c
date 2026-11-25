#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <time.h>

#define PORT 9090
#define MAX_MSG_LEN 1024
#define NICK_LEN 32

/* Префиксы протокола — простой текстовый формат */
#define MSG_JOIN    "JOIN"
#define MSG_LEAVE   "LEAVE"
#define MSG_CHAT    "CHAT"

static int sock_fd = -1;
static volatile sig_atomic_t running = 1;
static char my_nick[NICK_LEN];
static char bcast_addr[INET_ADDRSTRLEN] = "255.255.255.255";

/* ---------- Вспомогательные функции ---------- */

/* Пытаемся вычислить направленный широковещательный адрес подсети.
 * Если не удалось — остаётся 255.255.255.255 (limited broadcast). */
static void detect_broadcast_addr(void)
{
    int tmp = socket(AF_INET, SOCK_DGRAM, 0);
    if (tmp < 0) return;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    /* Обычно основной интерфейс называется eth0, enp0s3, wlan0 и т.п.
     * Можно перебрать, но для учебной задачи попробуем несколько имён. */
    const char *names[] = {"eth0", "enp0s3", "wlan0", "ens33", NULL};
    for (int i = 0; names[i]; i++) {
        strncpy(ifr.ifr_name, names[i], IFNAMSIZ - 1);
        if (ioctl(tmp, SIOCGIFBRDADDR, &ifr) == 0) {
            struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_broadaddr;
            if (sin->sin_family == AF_INET) {
                inet_ntop(AF_INET, &sin->sin_addr, bcast_addr, sizeof(bcast_addr));
                fprintf(stderr, "[i] broadcast address: %s (iface %s)\n",
                        bcast_addr, names[i]);
                close(tmp);
                return;
            }
        }
    }
    fprintf(stderr, "[i] broadcast address: %s (fallback)\n", bcast_addr);
    close(tmp);
}

/* Отправка широковещательной датаграммы */
static void send_bcast(const char *text)
{
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    inet_pton(AF_INET, bcast_addr, &dest.sin_addr);

    if (sendto(sock_fd, text, strlen(text), 0,
               (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("sendto");
    }
}

/* Упаковка сообщения: "PREFIX|nick|payload" */
static void send_proto(const char *prefix, const char *payload)
{
    char buf[MAX_MSG_LEN];
    snprintf(buf, sizeof(buf), "%s|%s|%s", prefix, my_nick,
             payload ? payload : "");
    send_bcast(buf);
}

/* ---------- Поток приёма ---------- */
static void *recv_thread(void *arg)
{
    (void)arg;
    char buf[MAX_MSG_LEN];

    while (running) {
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);
        ssize_t n = recvfrom(sock_fd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&sender, &slen);
        if (n < 0) {
            if (errno == EINTR) continue;      /* сигнал */
            perror("recvfrom");
            break;
        }
        buf[n] = '\0';

        /* Игнорируем собственныеbroadcast-копии */
        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender.sin_addr, sender_ip, sizeof(sender_ip));

        char *sep1 = strchr(buf, '|');
        if (!sep1) continue;
        *sep1 = '\0';
        char *prefix = buf;
        char *rest = sep1 + 1;

        char *sep2 = strchr(rest, '|');
        if (!sep2) continue;
        *sep2 = '\0';
        char *nick = rest;
        char *payload = sep2 + 1;

        if (strcmp(prefix, MSG_JOIN) == 0) {
            printf("\n*** %s joined the chat ***\n", nick);
        } else if (strcmp(prefix, MSG_LEAVE) == 0) {
            printf("\n*** %s left the chat ***\n", nick);
        } else if (strcmp(prefix, MSG_CHAT) == 0) {
            printf("\n[%s]: %s\n", nick, payload);
        } else {
            /* Неизвестный префикс — просто игнорируем */
            continue;
        }
        fflush(stdout);
        printf("> ");
        fflush(stdout);
    }
    return NULL;
}

/* ---------- Обработка сигналов ---------- */
static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

/* ---------- main ---------- */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <nickname>\n", argv[0]);
        return 1;
    }
    snprintf(my_nick, sizeof(my_nick), "%s", argv[1]);

    /* 1. Создаём UDP-сокет */
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    /* 2. Разрешаем широковещательную отправку */
    int on = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
        perror("setsockopt SO_BROADCAST");
        close(sock_fd);
        return 1;
    }

    /* 3. Позволяем нескольким экземплярам слушать один и тот же порт */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(sock_fd);
        return 1;
    }
#ifdef SO_REUSEPORT
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
        perror("setsockopt SO_REUSEPORT"); /* не критично, но выведем */
    }
#endif

    /* 4. Привязываемся к INADDR_ANY:PORT */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return 1;
    }

    /* 5. Пытаемся определить широковещательный адрес подсети */
    detect_broadcast_addr();

    /* 6. Обработчики сигналов для корректного выхода */
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* 7. Поток приёма */
    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create");
        close(sock_fd);
        return 1;
    }

    /* 8. Анонс о подключении */
    send_proto(MSG_JOIN, NULL);
    printf("UDP chat started as '%s' on port %d\n", my_nick, PORT);
    printf("Broadcast address: %s\n", bcast_addr);
    printf("Type messages and press Enter. Ctrl+C to quit.\n> ");
    fflush(stdout);

    /* 9. Главный поток: чтение строк из stdin и отправка */
    char line[MAX_MSG_LEN];
    while (running) {
        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) break;
            continue;
        }
        /* Убираем перевод строки */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }
        send_proto(MSG_CHAT, line);
        printf("> ");
        fflush(stdout);
    }

    /* 10. Анонс об отключении */
    send_proto(MSG_LEAVE, NULL);
    /* Дадим немного времени на отправку (broadcast не гарантирует доставку,
     * но для локальной сети обычно успевает). */
    usleep(100000);

    running = 0;
    pthread_join(tid, NULL);
    close(sock_fd);
    printf("\nChat closed.\n");
    return 0;
}