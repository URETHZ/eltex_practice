#define _GNU_SOURCE
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t stop_flag = 0;
static void on_stop(int sig) { (void)sig; stop_flag = 1; }

int subscriber_main(int topic_count, char *topics[])
{
    int qid = msgget(QUEUE_KEY, 0);
    if (qid < 0) {
        fprintf(stderr, "Subscriber: очередь не найдена.\n");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    pid_t mypid = getpid();
    fprintf(stderr, "Subscriber: запущен, pid=%d, тем: %d\n",
            (int)mypid, topic_count);

    /* Подписка */
    char text[MAX_TEXT];
    for (int i = 0; i < topic_count; i++) {
        if (build_msg(text, sizeof(text), "subscribe", mypid, topics[i], NULL) < 0)
            continue;
        if (mq_send(qid, MTYPE_BROKER, text) < 0) {
            perror("Subscriber: subscribe");
            return 1;
        }
        fprintf(stderr, "Subscriber: подписан на '%s'\n", topics[i]);
    }

    /* Приём сообщений с приоритетом = свой pid */
    struct msg_buffer buf;
    while (!stop_flag) {
        ssize_t n = mq_recv_type(qid, (long)mypid, &buf);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EIDRM || errno == EINVAL) {
                fprintf(stderr, "Subscriber: очередь удалена, выход.\n");
                break;
            }
            perror("Subscriber: msgrcv");
            break;
        }
        if (stop_flag) break;

        struct parsed_msg pm;
        if (parse_msg(buf.mtext, &pm) < 0) continue;
        if (strcmp(pm.kind, "message") != 0) continue;

        printf("[Subscriber %d] topic='%s' payload='%s'\n",
               (int)mypid, pm.topic, pm.payload ? pm.payload : "");
        fflush(stdout);
    }

    /* Отписка (best-effort) */
    for (int i = 0; i < topic_count; i++) {
        if (build_msg(text, sizeof(text), "unsubscribe", mypid, topics[i], NULL) >= 0)
            mq_send(qid, MTYPE_BROKER, text);   /* ошибки игнорируем */
    }

    fprintf(stderr, "Subscriber: завершение, pid=%d\n", (int)mypid);
    return 0;
}
