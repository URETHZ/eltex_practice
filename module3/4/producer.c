/* Производитель.

   Периодически создаёт набор случайного количества случайных чисел и дописывает
   его в конец односвязного списка в разделяемой памяти. Когда память исчерпана,
   отмечает, что генерация закончена, и ждёт, пока потребители обработают все
   наборы. После этого отключает и удаляет разделяемую память и семафор.

   Запуск: ./producer [-s размер_памяти] [-m макс_чисел_в_наборе] [-d пауза_мс] */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include "common.h"

#define DEFAULT_SHM_SIZE    1024    /* байтов */
#define DEFAULT_MAX_NUMBERS 10      /* максимум чисел в одном наборе */
#define DEFAULT_DELAY_MS    200     /* пауза между наборами и между проверками */

#define MIN_SHM_SIZE 64
#define MAX_SHM_SIZE (16L * 1024 * 1024)

union semun {                       /* в Linux эту структуру нужно объявить самим */
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
            "Usage: %s [-s shm_size] [-m max_numbers] [-d delay_ms]\n"
            "  -s  shared memory size in bytes, %d..%ld (default %d)\n"
            "  -m  maximum numbers in one set, 1..1000 (default %d)\n"
            "  -d  pause between sets and between checks, ms (default %d)\n",
            prog, MIN_SHM_SIZE, MAX_SHM_SIZE, DEFAULT_SHM_SIZE, DEFAULT_MAX_NUMBERS, DEFAULT_DELAY_MS);
}

/* Удаляет объекты, оставшиеся от прежнего запуска, который не успел убрать за собой
   (например, был убит). Объекты, к которым кто-то подключён, не трогаем.
   0 - можно создавать заново, -1 - память используется другим процессом. */
static int remove_stale_objects(key_t key)
{
    int removed = 0;

    int shmid = shmget(key, 0, 0600);
    if (shmid >= 0) {
        struct shmid_ds ds;
        if (shmctl(shmid, IPC_STAT, &ds) == 0 && ds.shm_nattch > 0) return -1;
        shmctl(shmid, IPC_RMID, NULL);
        removed = 1;
    }
    int semid = semget(key, 1, 0600);
    if (semid >= 0) {
        semctl(semid, 0, IPC_RMID);
        removed = 1;
    }
    if (removed) printf("[producer] removed stale shared memory / semaphore left by a previous run\n");
    return 0;
}

static void remove_ipc(void *shm, int shmid, int semid)
{
    if (shm) shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}

/* Сколько наборов ещё не обработано (вызывать под семафором) */
static unsigned count_unprocessed(void *shm)
{
    const ShmHeader *hdr = (const ShmHeader *)shm;
    unsigned pending = 0;

    for (uint32_t off = hdr->first; off != 0; ) {
        BlockHeader *b = block_at(shm, off);
        if (b->count > 0) pending++;
        off = b->next;
    }
    return pending;
}

/* Дописывает один набор в конец списка. Вызывать под семафором.
   Возвращает 0 и заполняет *offset, *count, либо -1, если память исчерпана. */
static int append_set(void *shm, int32_t max_numbers, uint32_t *offset_out, int32_t *count_out)
{
    ShmHeader *hdr = (ShmHeader *)shm;
    uint32_t remaining = hdr->total_size - hdr->used;

    if (remaining < MIN_BLOCK_SIZE) return -1;          /* нет места даже для одного числа */

    /* Сколько чисел поместится в оставшееся место */
    int32_t fits = (int32_t)((remaining - sizeof(BlockHeader)) / sizeof(int32_t));
    int32_t limit = fits < max_numbers ? fits : max_numbers;
    int32_t count = 1 + rand() % limit;                 /* случайное количество: 1..limit */

    uint32_t offset = hdr->used;
    BlockHeader *block = block_at(shm, offset);
    block->count = count;
    block->next = 0;                                    /* пока этот блок последний */
    int32_t *data = block_data(block);
    for (int32_t i = 0; i < count; i++)
        data[i] = rand() % 2001 - 1000;                 /* случайные числа -1000..1000 */

    /* Подцепляем блок к списку: предыдущий последний теперь указывает на него */
    if (hdr->last != 0) block_at(shm, hdr->last)->next = offset;
    else                hdr->first = offset;
    hdr->last = offset;
    hdr->used += (uint32_t)block_size(count);
    hdr->blocks++;

    *offset_out = offset;
    *count_out = count;
    return 0;
}

int main(int argc, char *argv[])
{
    long shm_size = DEFAULT_SHM_SIZE, max_numbers = DEFAULT_MAX_NUMBERS, delay_ms = DEFAULT_DELAY_MS;
    int opt;

    opterr = 0;
    while ((opt = getopt(argc, argv, "s:m:d:h")) != -1) {
        switch (opt) {
        case 's':
            if (!parse_long(optarg, MIN_SHM_SIZE, MAX_SHM_SIZE, &shm_size)) {
                fprintf(stderr, "%s: invalid size '%s' (%d..%ld)\n", argv[0], optarg, MIN_SHM_SIZE, MAX_SHM_SIZE);
                return 2;
            }
            break;
        case 'm':
            if (!parse_long(optarg, 1, 1000, &max_numbers)) {
                fprintf(stderr, "%s: invalid number count '%s' (1..1000)\n", argv[0], optarg);
                return 2;
            }
            break;
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
            if (optopt == 's' || optopt == 'm' || optopt == 'd')
                fprintf(stderr, "%s: option -%c requires an argument\n", argv[0], optopt);
            else
                fprintf(stderr, "%s: unknown option -%c\n", argv[0], optopt);
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
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    /* ---- создание разделяемой памяти и семафора ---- */
    key_t key = IPC_KEY_VALUE;

    if (remove_stale_objects(key) != 0) {
        fprintf(stderr, "%s: the shared memory is in use: another producer or consumers are attached\n", argv[0]);
        return 1;
    }

    int shmid = shmget(key, (size_t)shm_size, IPC_CREAT | IPC_EXCL | 0600);
    if (shmid < 0) {
        fprintf(stderr, "%s: cannot create shared memory: %s\n", argv[0], strerror(errno));
        return 1;
    }
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);    /* новый семафор имеет значение 0 */
    if (semid < 0) {
        fprintf(stderr, "%s: cannot create semaphore: %s\n", argv[0], strerror(errno));
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    void *shm = shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        fprintf(stderr, "%s: cannot attach shared memory: %s\n", argv[0], strerror(errno));
        remove_ipc(NULL, shmid, semid);
        return 1;
    }

    /* Инициализируем память, пока семафор равен 0: потребители, успевшие подключиться,
       ждут на семафоре и не увидят недозаполненный заголовок. Потом открываем доступ. */
    memset(shm, 0, (size_t)shm_size);
    ShmHeader *hdr = (ShmHeader *)shm;
    hdr->total_size = (uint32_t)shm_size;
    hdr->used = sizeof(ShmHeader);

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) < 0) {
        fprintf(stderr, "%s: cannot initialize semaphore: %s\n", argv[0], strerror(errno));
        remove_ipc(shm, shmid, semid);
        return 1;
    }

    printf("[producer] pid %d: shared memory %ld bytes, up to %ld numbers per set, pause %ld ms\n",
           (int)getpid(), shm_size, max_numbers, delay_ms);

    /* ---- генерация наборов, пока не исчерпана память ---- */
    unsigned long sets = 0, numbers = 0;

    while (!g_stop) {
        if (sem_lock(semid) != 0) break;

        uint32_t offset;
        int32_t count;
        int exhausted = (append_set(shm, (int32_t)max_numbers, &offset, &count) != 0);

        if (!exhausted) {
            sets++;
            numbers += (unsigned long)count;
            printf("[producer] set #%lu at offset %u: %d number(s):", sets, offset, count);
            const int32_t *data = block_data(block_at(shm, offset));
            for (int32_t i = 0; i < count; i++) printf(" %d", data[i]);
            printf("\n");
        }
        sem_unlock(semid);

        if (exhausted) break;
        sleep_ms(delay_ms);
    }

    if (g_stop) {
        printf("[producer] interrupted, removing shared memory and semaphore\n");
        remove_ipc(shm, shmid, semid);
        return 1;
    }

    /* Сообщаем потребителям, что новых наборов не будет */
    if (sem_lock(semid) == 0) {
        hdr->producer_done = 1;
        sem_unlock(semid);
    }
    printf("[producer] shared memory is exhausted: %lu sets, %lu numbers. Waiting until all sets are processed\n",
           sets, numbers);

    /* ---- ждём, пока все наборы будут обработаны ---- */
    for (;;) {
        if (sem_lock(semid) != 0) break;
        unsigned pending = count_unprocessed(shm);
        sem_unlock(semid);

        if (pending == 0) break;
        printf("[producer] %u set(s) are not processed yet, sleeping\n", pending);
        sleep_ms(delay_ms > 0 ? delay_ms : 1);
        if (g_stop) break;
    }

    /* ---- ждём, пока потребители отключатся от памяти ---- */
    while (!g_stop) {
        struct shmid_ds ds;
        if (shmctl(shmid, IPC_STAT, &ds) != 0 || ds.shm_nattch <= 1) break;     /* 1 - это мы сами */
        sleep_ms(delay_ms > 0 ? delay_ms : 1);
    }

    int status = g_stop ? 1 : 0;
    if (g_stop) printf("[producer] interrupted\n");
    else        printf("[producer] all sets are processed\n");

    remove_ipc(shm, shmid, semid);
    printf("[producer] shared memory detached; shared memory and semaphore removed\n");
    return status;
}
