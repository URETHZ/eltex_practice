#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stddef.h>

#define MAX_TEXT   1024
#define MAX_TOPIC  64

/* Один и тот же ключ очереди для всех процессов */
#define QUEUE_KEY ((key_t)0xB0B0)

/* Приоритет сообщений, читаемых брокером */
#define MTYPE_BROKER 1L

struct msg_buffer {
    long mtype;
    char mtext[MAX_TEXT];
};

/* Разобранное сообщение протокола */
struct parsed_msg {
    char kind[32];
    pid_t pid;
    char topic[MAX_TOPIC];
    const char *payload;   /* указатель внутрь исходного буфера */
};

/* Сборка сообщения "kind,pid,topic[,payload]" */
int build_msg(char *buf, size_t bufsz, const char *kind, pid_t pid,
              const char *topic, const char *payload);

/* Сборка сообщения "kind,topic,payload" (брокер→подписчик) */
int build_msg_broker(char *buf, size_t bufsz, const char *kind,
                     const char *topic, const char *payload);

/* Разбор сообщения. Возвращает 0 при успехе, -1 при ошибке. */
int parse_msg(char *buf, struct parsed_msg *out);

/* Отправка: msgsnd с указанным mtype */
int mq_send(int qid, long mtype, const char *text);

/* Приём: msgrcv с указанным msgtyp */
ssize_t mq_recv_type(int qid, long mtype, struct msg_buffer *buf);

#endif
