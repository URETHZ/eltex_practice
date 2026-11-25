#ifndef COMMON_H
#define COMMON_H

/* Общее для производителя и потребителей: ключ, раскладка разделяемой памяти,
   семафор-мьютекс. */

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Общий ключ разделяемой памяти и семафора (оба процесса знают его заранее) */
#define IPC_KEY_VALUE ((key_t)0x53484D34)

/* ---------- Раскладка разделяемой памяти ----------

   смещение 0                    ShmHeader
   смещение sizeof(ShmHeader)    первый блок данных
   ...                           следующие блоки подряд

   Блок = BlockHeader + count чисел int32_t. Блоки образуют односвязный список:
   поле next хранит СМЕЩЕНИЕ следующего блока от начала сегмента (не указатель:
   в каждом процессе сегмент подключён по своему адресу, а смещение одинаково
   для всех). next == 0 - блок последний. */

typedef struct {
    uint32_t total_size;      /* размер сегмента в байтах */
    uint32_t used;            /* смещение первого свободного байта */
    uint32_t first;           /* смещение первого блока; 0 - блоков ещё нет */
    uint32_t last;            /* смещение последнего блока (чтобы добавлять в конец) */
    uint32_t blocks;          /* сколько блоков создано */
    uint32_t producer_done;   /* 1 - производитель закончил генерацию */
} ShmHeader;

typedef struct {
    int32_t count;            /* число элементов в наборе; 0 - набор уже обработан */
    uint32_t next;            /* смещение следующего блока; 0 - последний */
} BlockHeader;

/* Минимальный блок: заголовок и одно число */
#define MIN_BLOCK_SIZE (sizeof(BlockHeader) + sizeof(int32_t))

static inline size_t block_size(int32_t count)
{
    return sizeof(BlockHeader) + (size_t)count * sizeof(int32_t);
}

static inline BlockHeader *block_at(void *shm, uint32_t offset)
{
    return (BlockHeader *)((char *)shm + offset);
}

static inline int32_t *block_data(BlockHeader *block)
{
    return (int32_t *)(block + 1);
}

/* ---------- Сигналы ---------- */

/* Поднимается обработчиком SIGINT и SIGTERM; основные циклы его проверяют */
extern volatile sig_atomic_t g_stop;
void install_stop_handlers(void);

/* ---------- Семафор ---------- */

/* Семафор используется как мьютекс: 1 - память свободна, 0 - занята.
   Операции выполняются с SEM_UNDO: если процесс погибнет, держа семафор,
   ядро само вернёт значение, и остальные не зависнут навсегда. */
int sem_lock(int semid);      /* 0 - успех; -1 - ошибка (EINTR при g_stop, EIDRM/EINVAL - семафор удалён) */
int sem_unlock(int semid);

/* Сон в миллисекундах; прерывается сигналом завершения */
void sleep_ms(long ms);

/* Разбор числа из аргумента командной строки. 1 - успех. */
int parse_long(const char *text, long min, long max, long *out);

#endif /* COMMON_H */
