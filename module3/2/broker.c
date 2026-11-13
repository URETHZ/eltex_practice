#define _GNU_SOURCE
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#define MAX_SUBS        64
#define MAX_PUBS        64
#define MAX_SUB_TOPICS  16

struct subscriber {
    pid_t pid;
    char topics[MAX_SUB_TOPICS][MAX_TOPIC];
    int topic_count;
};

static struct subscriber subs[MAX_SUBS];
static int  sub_count = 0;
static pid_t pubs[MAX_PUBS];
static int  pub_count = 0;

static int qid_global = -1;
static volatile sig_atomic_t stop_flag = 0;

static void on_stop(int sig) { (void)sig; stop_flag = 1; }

/* --- Работа со списком подписчиков ---------------------------- */

static int find_sub(pid_t pid) {
    for (int i = 0; i < sub_count; i++)
        if (subs[i].pid == pid) return i;
    return -1;
}

static int ensure_sub(pid_t pid) {
    int idx = find_sub(pid);
    if (idx >= 0) return idx;
    if (sub_count >= MAX_SUBS) return -1;
    subs[sub_count].pid = pid;
    subs[sub_count].topic_count = 0;
    return sub_count++;
}

static int sub_has_topic(int idx, const char *topic) {
    for (int i = 0; i < subs[idx].topic_count; i++)
        if (strcmp(subs[idx].topics[i], topic) == 0) return 1;
    return 0;
}

static void sub_add_topic(int idx, const char *topic) {
    if (sub_has_topic(idx, topic)) return;
    if (subs[idx].topic_count >= MAX_SUB_TOPICS) return;
    strncpy(subs[idx].topics[subs[idx].topic_count], topic, MAX_TOPIC - 1);
    subs[idx].topics[subs[idx].topic_count][MAX_TOPIC - 1] = '\0';
    subs[idx].topic_count++;
}

static void sub_del_topic(int idx, const char *topic) {
    for (int i = 0; i < subs[idx].topic_count; i++) {
        if (strcmp(subs[idx].topics[i], topic) == 0) {
            for (int j = i; j < subs[idx].topic_count - 1; j++)
                strcpy(subs[idx].topics[j], subs[idx].topics[j + 1]);
            subs[idx].topic_count--;
            return;
        }
    }
}

/* --- Работа со списком издателей ------------------------------ */

static void add_pub(pid_t pid) {
    for (int i = 0; i < pub_count; i++)
        if (pubs[i] == pid) return;
    if (pub_count < MAX_PUBS) pubs[pub_count++] = pid;
}

/* --- Обработчики протокола ------------------------------------ */

static void handle_send(const char *topic, const char *payload, pid_t pub_pid)
{
    add_pub(pub_pid);

    char text[MAX_TEXT];
    if (build_msg_broker(text, sizeof(text), "message", topic, payload) < 0) {
        fprintf(stderr, "Broker: payload слишком длинный\n");
        return;
    }
    for (int i = 0; i < sub_count; i++) {
        if (sub_has_topic(i, topic)) {
            if (mq_send(qid_global, subs[i].pid, text) < 0) {
                /* Подписчик мог уже умереть — не критично */
            }
        }
    }
    fprintf(stderr, "Broker: сообщение по теме '%s' разослано\n", topic);
}
static void notify_shutdown(void)
{
    fprintf(stderr, "Broker: рассылка SIGINT подписчикам и издателям\n");
    for (int i = 0; i < sub_count; i++)
        if (subs[i].pid > 1) kill(subs[i].pid, SIGINT);
    for (int i = 0; i < pub_count; i++)
        if (pubs[i] > 1) kill(pubs[i], SIGINT);
}

/* --- Главная функция брокера --------------------------------- */

int broker_main(int timeout_sec)
{
    int qid = msgget(QUEUE_KEY, IPC_CREAT | IPC_EXCL | 0666);
    if (qid < 0) {
        if (errno == EEXIST) {
            fprintf(stderr, "Broker: очередь уже создана другим процессом.\n");
        } else {
            perror("Broker: msgget");
        }
        return 1;
    }
    qid_global = qid;
    fprintf(stderr, "Broker: запущен, qid=%d, pid=%d\n", qid, (int)getpid());

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sa.sa_flags = 0;   /* без SA_RESTART — чтобы прервать msgrcv */
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);

    if (timeout_sec > 0) {
        alarm((unsigned)timeout_sec);
        fprintf(stderr, "Broker: таймаут %d сек\n", timeout_sec);
    }

    struct msg_buffer buf;
    while (!stop_flag) {
        ssize_t n = mq_recv_type(qid, MTYPE_BROKER, &buf);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EIDRM || errno == EINVAL) {
                fprintf(stderr, "Broker: очередь удалена извне, выход.\n");
                return 0;
            }
            perror("Broker: msgrcv");
            break;
        }
        if (timeout_sec > 0) alarm((unsigned)timeout_sec);

        struct parsed_msg pm;
        if (parse_msg(buf.mtext, &pm) < 0) {
            fprintf(stderr, "Broker: некорректное сообщение\n");
            continue;
        }

        if (strcmp(pm.kind, "send") == 0 && pm.payload) {
            handle_send(pm.topic, pm.payload, pm.pid);
        } else if (strcmp(pm.kind, "subscribe") == 0) {
            int idx = ensure_sub(pm.pid);
            if (idx >= 0) {
                sub_add_topic(idx, pm.topic);
                fprintf(stderr, "Broker: подписка pid=%d на '%s'\n",
                        (int)pm.pid, pm.topic);
            }
        } else if (strcmp(pm.kind, "unsubscribe") == 0) {
            int idx = find_sub(pm.pid);
            if (idx >= 0) {
                sub_del_topic(idx, pm.topic);
                fprintf(stderr, "Broker: отписка pid=%d от '%s'\n",
                        (int)pm.pid, pm.topic);
            }
        }
    }

    notify_shutdown();
    msgctl(qid, IPC_RMID, NULL);
    fprintf(stderr, "Broker: очередь удалена, завершение.\n");
    return 0;
}
