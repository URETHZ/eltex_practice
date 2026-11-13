#define _GNU_SOURCE
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static volatile sig_atomic_t stop_flag = 0;
static void on_stop(int sig) { (void)sig; stop_flag = 1; }

int publisher_main(const char *topic)
{
    int qid = msgget(QUEUE_KEY, 0);
    if (qid < 0) {
        fprintf(stderr, "Publisher: очередь не найдена (брокер не запущен?).\n");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    fprintf(stderr, "Publisher: запущен, pid=%d, topic='%s'\n",
            (int)getpid(), topic);

    char text[MAX_TEXT];
    char payload[MAX_TEXT];
    int counter = 0;

    while (!stop_flag) {
        snprintf(payload, sizeof(payload),
                 "message #%d from publisher %d (topic=%s)",
                 ++counter, (int)getpid(), topic);

        if (build_msg(text, sizeof(text), "send", getpid(), topic, payload) < 0) {
            fprintf(stderr, "Publisher: сообщение слишком длинное\n");
            break;
        }

        if (mq_send(qid, MTYPE_BROKER, text) < 0) {
            if (errno == EIDRM || errno == EINVAL) {
                fprintf(stderr, "Publisher: очередь удалена, выход.\n");
                break;
            }
            perror("Publisher: msgsnd");
            break;
        }

        /* Раз в секунду. nanosleep прерывается сигналом — ок. */
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
    }

    fprintf(stderr, "Publisher: завершение, pid=%d\n", (int)getpid());
    return 0;
}
