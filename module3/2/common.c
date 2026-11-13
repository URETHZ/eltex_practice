#define _GNU_SOURCE
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int build_msg(char *buf, size_t bufsz, const char *kind, pid_t pid,
              const char *topic, const char *payload)
{
    int n;
    if (payload)
        n = snprintf(buf, bufsz, "%s,%d,%s,%s",
                     kind, (int)pid, topic, payload);
    else
        n = snprintf(buf, bufsz, "%s,%d,%s", kind, (int)pid, topic);
    if (n < 0 || (size_t)n >= bufsz) return -1;
    return n;
}

int build_msg_broker(char *buf, size_t bufsz, const char *kind,
                     const char *topic, const char *payload)
{
    int n;
    if (payload)
        n = snprintf(buf, bufsz, "%s,%s,%s", kind, topic, payload);
    else
        n = snprintf(buf, bufsz, "%s,%s", kind, topic);
    if (n < 0 || (size_t)n >= bufsz) return -1;
    return n;
}

static void copy_str(char *dst, size_t dstsz, const char *src, size_t len)
{
    if (len >= dstsz) len = dstsz - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

int parse_msg(char *buf, struct parsed_msg *out)
{
    memset(out, 0, sizeof(*out));

    char *c1 = strchr(buf, ',');
    if (!c1) return -1;

    copy_str(out->kind, sizeof(out->kind), buf, (size_t)(c1 - buf));
    char *p = c1 + 1;

    /* subscribe / unsubscribe / send */
    if (strcmp(out->kind, "subscribe") == 0 ||
        strcmp(out->kind, "unsubscribe") == 0 ||
        strcmp(out->kind, "send") == 0)
    {
        char *c2 = strchr(p, ',');
        if (!c2) return -1;
        *c2 = '\0';
        out->pid = (pid_t)atoi(p);
        p = c2 + 1;

        char *c3 = strchr(p, ',');
        if (c3) {
            *c3 = '\0';
            copy_str(out->topic, sizeof(out->topic), p, strlen(p));
            out->payload = c3 + 1;
        } else {
            copy_str(out->topic, sizeof(out->topic), p, strlen(p));
            out->payload = NULL;
        }
        return 0;
    }

    /* message,topic,payload  (брокер → подписчик) */
    if (strcmp(out->kind, "message") == 0) {
        char *c2 = strchr(p, ',');
        if (!c2) return -1;
        *c2 = '\0';
        copy_str(out->topic, sizeof(out->topic), p, strlen(p));
        out->payload = c2 + 1;
        return 0;
    }

    /* shutdown */
    if (strcmp(out->kind, "shutdown") == 0) {
        return 0;
    }

    return -1;
}

int mq_send(int qid, long mtype, const char *text)
{
    struct msg_buffer m;
    size_t len = strlen(text);
    if (len >= MAX_TEXT) {
        errno = EMSGSIZE;
        return -1;
    }
    m.mtype = mtype;
    memcpy(m.mtext, text, len);
    if (msgsnd(qid, &m, len, 0) < 0) return -1;
    return 0;
}

ssize_t mq_recv_type(int qid, long mtype, struct msg_buffer *buf)
{
    ssize_t n = msgrcv(qid, buf, MAX_TEXT - 1, mtype, 0);
    if (n < 0) return -1;
    buf->mtext[n] = '\0';
    return n;
}
