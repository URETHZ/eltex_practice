#ifndef PQUEUE_H
#define PQUEUE_H

#include <stddef.h>

/* Приоритет - целое число от 0 до 255. Чем больше число, тем выше приоритет.
   Порядок в очереди: сначала элементы с наибольшим приоритетом,
   а среди равных по приоритету - в порядке добавления (FIFO). */
#define PQ_MIN_PRIORITY 0
#define PQ_MAX_PRIORITY 255
#define PQ_LEVELS 256

typedef enum {
    PQ_OK = 0,
    PQ_NOT_FOUND,      /* очередь пуста или нет подходящего элемента */
    PQ_ERR_PRIORITY,   /* приоритет вне диапазона 0..255 */
    PQ_ERR_MEMORY,     /* не хватило памяти */
    PQ_ERR_ARGUMENT    /* неверный аргумент (например, NULL вместо out) */
} PqStatus;

/* Узел односвязной очереди одного уровня приоритета */
typedef struct PqNode {
    void *data;
    struct PqNode *next;
} PqNode;

/* Все элементы одного приоритета: обычная FIFO-очередь */
typedef struct {
    PqNode *head;      /* откуда извлекаем */
    PqNode *tail;      /* куда добавляем */
    size_t count;
} PqLevel;

/* Очередь с приоритетом: по одной FIFO-очереди на каждое из 256 значений приоритета */
typedef struct {
    PqLevel levels[PQ_LEVELS];
    size_t size;
} PriorityQueue;

/* Извлечённый элемент */
typedef struct {
    int priority;
    void *data;
} PqItem;

/* Вызывается для каждого элемента при обходе очереди */
typedef void (*PqVisitor)(int priority, void *data, void *ctx);

void pq_init(PriorityQueue *pq);

/* ---- Добавление ---- */
PqStatus pq_push(PriorityQueue *pq, int priority, void *data);

/* ---- Извлечение (элемент удаляется из очереди и возвращается в *out) ---- */

/* 1. Первый в очереди: наибольший приоритет, среди равных - самый старый. */
PqStatus pq_pop(PriorityQueue *pq, PqItem *out);

/* 2. С указанным приоритетом: самый старый элемент с ровно этим приоритетом. */
PqStatus pq_pop_priority(PriorityQueue *pq, int priority, PqItem *out);

/* 3. С приоритетом не ниже заданного: первый в очереди элемент, если его
      приоритет >= min_priority. Иначе подходящих элементов нет (PQ_NOT_FOUND),
      потому что первый в очереди имеет наибольший приоритет. */
PqStatus pq_pop_at_least(PriorityQueue *pq, int min_priority, PqItem *out);

/* ---- Просмотр без удаления ---- */
PqStatus pq_peek(const PriorityQueue *pq, PqItem *out);        /* первый в очереди */
size_t pq_size(const PriorityQueue *pq);
size_t pq_count_priority(const PriorityQueue *pq, int priority); /* 0 при неверном приоритете */
void pq_for_each(const PriorityQueue *pq, PqVisitor visit, void *ctx);  /* в порядке извлечения */

/* Удаляет все элементы. destroy (может быть NULL) вызывается для data каждого элемента. */
void pq_clear(PriorityQueue *pq, void (*destroy)(void *));

const char *pq_status_message(PqStatus status);

#endif /* PQUEUE_H */
