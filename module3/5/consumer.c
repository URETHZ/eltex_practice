/* Потребитель (разделяемая память и семафор POSIX).

   Периодически просматривает список в разделяемой памяти, находит первый
   необработанный набор, определяет в нём максимальное и минимальное число,
   выводит их и помечает набор обработанным (count = 0). После каждого набора
   "засыпает" на заданное время. Когда производитель закончил генерацию и
   необработанных наборов не осталось, отключается от памяти и завершается.
   Можно запускать несколько потребителей одновременно.

   Запуск: ./consumer [-d пауза_мс] */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"

#define DEFAULT_DELAY_MS 500
#define WAIT_PRODUCER_MS 5000       /* сколько ждать появления памяти, если производитель ещё не запущен */
#define WAIT_STEP_MS     100

static void usage(FILE *out, const char *prog)
{
    fprintf(out, "Usage: %s [-d delay_ms]\n"
                 "  -d  pause after each processed set, ms (default %d)\n", prog, DEFAULT_DELAY_MS);
}

static int process_alive(pid_t pid)
{
    return kill(pid, 0) == 0 || errno == EPERM;
}

/* Результат поиска необработанного набора */
typedef struct {
    int found;
    uint32_t offset;
    int32_t count;
    int32_t min;
    int32_t max;
} Result;

/* Ищет первый необработанный набор, анализирует его и помечает обработанным.
   Вызывать под семафором. Возвращает 0, либо -1, если список повреждён. */
static int take_one_set(void *shm, Result *res)
{
    const ShmHeader *hdr = (const ShmHeader *)shm;
    res->found = 0;

    for (uint32_t off = hdr->first; off != 0; ) {
        /* Защита от повреждённого списка: смещение должно указывать внутрь сегмента */
        if (off < sizeof(ShmHeader) || (size_t)off + MIN_BLOCK_SIZE > hdr->total_size) return -1;

        BlockHeader *block = block_at(shm, off);
        if (block->count > 0) {
            const int32_t *data = block_data(block);
            if ((size_t)off + block_size(block->count) > hdr->total_size) return -1;

            int32_t min = data[0], max = data[0];
            for (int32_t i = 1; i < block->count; i++) {
                if (data[i] < min) min = data[i];
                if (data[i] > max) max = data[i];
            }

            res->found = 1;
            res->offset = off;
            res->count = block->count;
            res->min = min;
            res->max = max;
            block->count = 0;                   /* пометка "набор обработан" */
            return 0;
        }
        off = block->next;
    }
    return 0;
}

/* Открывает память и семафор. Если производитель ещё не создал их, немного ждём.
   0 - успех (заполняет *shm, *size, *sem), -1 - не удалось. */
static int connect_to_producer(void **shm, size_t *size, sem_t **sem)
{
    for (long waited = 0; !g_stop; waited += WAIT_STEP_MS) {
        int fd = shm_open(SHM_NAME, O_RDWR, 0);
        if (fd >= 0) {
            struct stat st;
            /* Производитель сначала создаёт и заполняет память, а семафор - в последнюю очередь.
               Поэтому, когда семафор существует, заголовок гарантированно готов. */
            if (fstat(fd, &st) == 0 && (size_t)st.st_size >= sizeof(ShmHeader)) {
                sem_t *s = sem_open(SEM_NAME, 0);
                if (s != SEM_FAILED) {
                    void *p = mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                    close(fd);
                    if (p == MAP_FAILED) {
                        sem_close(s);
                        return -1;
                    }
                    *shm = p;
                    *size = (size_t)st.st_size;
                    *sem = s;
                    return 0;
                }
            }
            close(fd);
        }
        if (waited >= WAIT_PRODUCER_MS) break;
        sleep_ms(WAIT_STEP_MS);
    }
    return -1;
}

int main(int argc, char *argv[])
{
    long delay_ms = DEFAULT_DELAY_MS;
    int opt;

    opterr = 0;
    while ((opt = getopt(argc, argv, "d:h")) != -1) {
        switch (opt) {
        case 'd':
            if (!parse_long(optarg, 0, 60000, &delay_ms)) {
                fprintf(stderr, "%s: invalid delay '%s' (0..60000 ms)\n", argv[0], optarg);
                return 2;
            }
            break;
        case 'h':
            usage(stdout, argv[0]);
            return 0;
        default:
            if (optopt == 'd') fprintf(stderr, "%s: option -d requires an argument\n", argv[0]);
            else               fprintf(stderr, "%s: unknown option -%c\n", argv[0], optopt);
            usage(stderr, argv[0]);
            return 2;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "%s: unexpected argument '%s'\n", argv[0], argv[optind]);
        usage(stderr, argv[0]);
        return 2;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    install_stop_handlers();

    void *shm = NULL;
    size_t size = 0;
    sem_t *sem = SEM_FAILED;

    if (connect_to_producer(&shm, &size, &sem) != 0) {
        if (g_stop) return 1;
        fprintf(stderr, "%s: the producer is not running (cannot open %s or %s)\n", argv[0], SHM_NAME, SEM_NAME);
        return 1;
    }
    int pid = (int)getpid();
    printf("[consumer %d] attached, pause %ld ms\n", pid, delay_ms);

    /* ---- основной цикл: периодически берём по одному набору ---- */
    int status = 0;
    unsigned long processed = 0;

    while (!g_stop) {
        if (lock_memory(sem) != 0) {
            if (errno == ETIMEDOUT) {
                fprintf(stderr, "%s: the semaphore was not released for %d s (did a process die holding it?)\n",
                        argv[0], LOCK_TIMEOUT_SEC);
                status = 1;
            }
            break;                              /* EINTR - нас просят завершиться */
        }

        Result res;
        int bad = (take_one_set(shm, &res) != 0);
        const ShmHeader *hdr = (const ShmHeader *)shm;
        int finished = hdr->producer_done;      /* читаем под тем же семафором */
        pid_t producer = (pid_t)hdr->producer_pid;
        unlock_memory(sem);

        if (bad) {
            fprintf(stderr, "%s: the data in shared memory is corrupted\n", argv[0]);
            status = 1;
            break;
        }

        if (res.found) {
            processed++;
            printf("[consumer %d] set at offset %u (%d number(s)): min = %d, max = %d\n",
                   pid, res.offset, res.count, res.min, res.max);
        } else if (finished) {
            printf("[consumer %d] all sets are processed\n", pid);
            break;
        } else if (producer != 0 && !process_alive(producer)) {
            /* Работы нет, а производитель погиб, не успев отметить конец: ждать нечего */
            printf("[consumer %d] the producer has terminated unexpectedly, exiting\n", pid);
            status = 1;
            break;
        }
        sleep_ms(delay_ms);                     /* после набора (или если работы пока нет) - пауза */
    }

    munmap(shm, size);
    sem_close(sem);
    printf("[consumer %d] detached from shared memory (%lu set(s) processed)\n", pid, processed);
    return status;
}
