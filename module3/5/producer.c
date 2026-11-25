/* Производитель (разделяемая память и семафор POSIX).

   Периодически создаёт набор случайного количества случайных чисел и дописывает
   его в конец односвязного списка в разделяемой памяти. Когда память исчерпана,
   отмечает, что генерация закончена, и ждёт, пока потребители обработают все
   наборы. После этого отключает и удаляет разделяемую память и семафор.

   Запуск: ./producer [-s размер_памяти] [-m макс_чисел_в_наборе] [-d пауза_мс] */

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
#include <time.h>
#include <unistd.h>
#include "common.h"

#define DEFAULT_SHM_SIZE    1024    /* байтов */
#define DEFAULT_MAX_NUMBERS 10      /* максимум чисел в одном наборе */
#define DEFAULT_DELAY_MS    200     /* пауза между наборами и между проверками */

#define MIN_SHM_SIZE 64
#define MAX_SHM_SIZE (16L * 1024 * 1024)

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
            "Usage: %s [-s shm_size] [-m max_numbers] [-d delay_ms]\n"
            "  -s  shared memory size in bytes, %d..%ld (default %d)\n"
            "  -m  maximum numbers in one set, 1..1000 (default %d)\n"
            "  -d  pause between sets and between checks, ms (default %d)\n",
            prog, MIN_SHM_SIZE, MAX_SHM_SIZE, DEFAULT_SHM_SIZE, DEFAULT_MAX_NUMBERS, DEFAULT_DELAY_MS);
}

static int process_alive(pid_t pid)
{
    return kill(pid, 0) == 0 || errno == EPERM;         /* EPERM - процесс есть, но чужой */
}

/* Удаляет объекты, оставшиеся от прежнего запуска, который не успел убрать за собой
   (например, был убит). В POSIX нет счётчика подключений, поэтому о том, что
   память кому-то принадлежит, судим по PID производителя в заголовке: если этот
   процесс жив, объекты не трогаем.
   0 - можно создавать заново, -1 - работает другой производитель. */
static int remove_stale_objects(void)
{
    int removed = 0;

    int fd = shm_open(SHM_NAME, O_RDONLY, 0);
    if (fd >= 0) {
        struct stat st;
        uint32_t pid = 0;

        if (fstat(fd, &st) == 0 && (size_t)st.st_size >= sizeof(ShmHeader)) {
            void *p = mmap(NULL, sizeof(ShmHeader), PROT_READ, MAP_SHARED, fd, 0);
            if (p != MAP_FAILED) {
                pid = ((const ShmHeader *)p)->producer_pid;
                munmap(p, sizeof(ShmHeader));
            }
        }
        close(fd);

        if (pid != 0 && process_alive((pid_t)pid)) return -1;
        shm_unlink(SHM_NAME);
        removed = 1;
    }

    sem_t *old = sem_open(SEM_NAME, 0);
    if (old != SEM_FAILED) {
        sem_close(old);
        sem_unlink(SEM_NAME);
        removed = 1;
    }

    if (removed) printf("[producer] removed stale shared memory / semaphore left by a previous run\n");
    return 0;
}

/* Отключает и удаляет разделяемую память и семафор */
static void remove_ipc(void *shm, size_t size, sem_t *sem)
{
    if (shm) munmap(shm, size);
    shm_unlink(SHM_NAME);
    if (sem != SEM_FAILED) sem_close(sem);
    sem_unlink(SEM_NAME);
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

    /* ---- создание разделяемой памяти ---- */
    if (remove_stale_objects() != 0) {
        fprintf(stderr, "%s: the shared memory is in use: another producer is running\n", argv[0]);
        return 1;
    }

    size_t size = (size_t)shm_size;
    int fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        fprintf(stderr, "%s: cannot create shared memory %s: %s\n", argv[0], SHM_NAME, strerror(errno));
        return 1;
    }
    if (ftruncate(fd, (off_t)size) < 0) {               /* новый объект имеет размер 0 - задаём нужный */
        fprintf(stderr, "%s: cannot set shared memory size: %s\n", argv[0], strerror(errno));
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }
    void *shm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);                                          /* после mmap дескриптор не нужен */
    if (shm == MAP_FAILED) {
        fprintf(stderr, "%s: cannot map shared memory: %s\n", argv[0], strerror(errno));
        shm_unlink(SHM_NAME);
        return 1;
    }

    /* Инициализируем память ДО создания семафора: потребители, найдя память,
       ждут появления семафора и не увидят недозаполненный заголовок. */
    memset(shm, 0, size);
    ShmHeader *hdr = (ShmHeader *)shm;
    hdr->total_size = (uint32_t)size;
    hdr->used = sizeof(ShmHeader);
    hdr->producer_pid = (uint32_t)getpid();

    /* Именованный семафор со значением 1: память свободна */
    sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 1);
    if (sem == SEM_FAILED) {
        fprintf(stderr, "%s: cannot create semaphore %s: %s\n", argv[0], SEM_NAME, strerror(errno));
        remove_ipc(shm, size, SEM_FAILED);
        return 1;
    }

    printf("[producer] pid %d: shared memory %ld bytes, up to %ld numbers per set, pause %ld ms\n",
           (int)getpid(), shm_size, max_numbers, delay_ms);

    /* ---- генерация наборов, пока не исчерпана память ---- */
    unsigned long sets = 0, numbers = 0;
    int lock_failed = 0;

    while (!g_stop) {
        if (lock_memory(sem) != 0) {
            lock_failed = (errno == ETIMEDOUT);
            break;
        }

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
        unlock_memory(sem);

        if (exhausted) break;
        sleep_ms(delay_ms);
    }

    if (lock_failed) {
        fprintf(stderr, "%s: the semaphore was not released for %d s (did a process die holding it?)\n",
                argv[0], LOCK_TIMEOUT_SEC);
        remove_ipc(shm, size, sem);
        return 1;
    }

    if (g_stop) {
        /* Говорим потребителям, что новых наборов не будет: они доработают то, что уже есть */
        if (lock_memory(sem) == 0) {
            hdr->producer_done = 1;
            unlock_memory(sem);
        }
        printf("[producer] interrupted, removing shared memory and semaphore\n");
        remove_ipc(shm, size, sem);
        return 1;
    }

    /* Сообщаем потребителям, что новых наборов не будет */
    if (lock_memory(sem) == 0) {
        hdr->producer_done = 1;
        unlock_memory(sem);
    }
    printf("[producer] shared memory is exhausted: %lu sets, %lu numbers. Waiting until all sets are processed\n",
           sets, numbers);

    /* ---- ждём, пока все наборы будут обработаны ---- */
    int status = 0;
    for (;;) {
        if (lock_memory(sem) != 0) {
            if (errno == ETIMEDOUT)
                fprintf(stderr, "%s: the semaphore was not released for %d s (did a process die holding it?)\n",
                        argv[0], LOCK_TIMEOUT_SEC);
            status = 1;
            break;
        }
        unsigned pending = count_unprocessed(shm);
        unlock_memory(sem);

        if (pending == 0) break;
        printf("[producer] %u set(s) are not processed yet, sleeping\n", pending);
        sleep_ms(delay_ms > 0 ? delay_ms : 1);
        if (g_stop) {
            status = 1;
            break;
        }
    }

    if (status == 0) printf("[producer] all sets are processed\n");
    else              printf("[producer] interrupted\n");

    /* Имена можно удалять, пока потребители ещё подключены: у них объекты
       остаются живыми, пока они сами их не закроют. */
    remove_ipc(shm, size, sem);
    printf("[producer] shared memory unmapped; shared memory and semaphore removed\n");
    return status;
}
