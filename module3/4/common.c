#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <time.h>
#include "common.h"

volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* Без SA_RESTART: сигнал прерывает semop() и nanosleep(), и мы сразу видим флаг */
void install_stop_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int sem_lock(int semid)
{
    struct sembuf op = { 0, -1, SEM_UNDO };     /* P: ждать, пока значение не станет 1, и уменьшить */

    for (;;) {
        if (semop(semid, &op, 1) == 0) return 0;
        if (errno == EINTR) {
            if (g_stop) return -1;              /* нас просят завершиться */
            continue;
        }
        return -1;
    }
}

int sem_unlock(int semid)
{
    struct sembuf op = { 0, +1, SEM_UNDO };     /* V: освободить */
    return semop(semid, &op, 1);
}

void sleep_ms(long ms)
{
    struct timespec req = { ms / 1000, (ms % 1000) * 1000000L };
    struct timespec rem;

    while (nanosleep(&req, &rem) == -1) {
        if (errno != EINTR || g_stop) return;
        req = rem;                              /* доспать остаток после постороннего сигнала */
    }
}

int parse_long(const char *text, long min, long max, long *out)
{
    if (text[0] == '\0') return 0;
    for (const char *p = text; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;

    errno = 0;
    char *end;
    long v = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || v < min || v > max) return 0;
    *out = v;
    return 1;
}
