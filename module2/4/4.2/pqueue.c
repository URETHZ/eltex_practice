#include <stdlib.h>
#include "pqueue.h"

static int valid_priority(int priority)
{
    return priority >= PQ_MIN_PRIORITY && priority <= PQ_MAX_PRIORITY;
}

/* Наибольший приоритет среди элементов очереди или -1, если она пуста */
static int highest_level(const PriorityQueue *pq)
{
    if (pq->size == 0) return -1;
    for (int p = PQ_MAX_PRIORITY; p >= PQ_MIN_PRIORITY; p--)
        if (pq->levels[p].count > 0) return p;
    return -1;
}

static int first_highest_level_from_min(const PriorityQueue *pq, int minPriority)
{
    if (pq->size == 0) return -1;
    for(int p = minPriority; p<=PQ_MAX_PRIORITY; p++)
        if (pq->levels[p].count > 0) return p;
    return -1;
}

/* Забирает самый старый элемент уровня priority. Уровень не должен быть пуст. */
static void take_head(PriorityQueue *pq, int priority, PqItem *out)
{
    PqLevel *level = &pq->levels[priority];
    PqNode *node = level->head;

    level->head = node->next;
    if (level->head == NULL) level->tail = NULL;
    level->count--;
    pq->size--;

    out->priority = priority;
    out->data = node->data;
    free(node);
}

void pq_init(PriorityQueue *pq)
{
    for (int p = 0; p < PQ_LEVELS; p++) {
        pq->levels[p].head = NULL;
        pq->levels[p].tail = NULL;
        pq->levels[p].count = 0;
    }
    pq->size = 0;
}

PqStatus pq_push(PriorityQueue *pq, int priority, void *data)
{
    if (!valid_priority(priority)) return PQ_ERR_PRIORITY;

    PqNode *node = (PqNode *)malloc(sizeof(PqNode));
    if (!node) return PQ_ERR_MEMORY;
    node->data = data;
    node->next = NULL;

    PqLevel *level = &pq->levels[priority];
    if (level->tail) level->tail->next = node;      /* в конец своей FIFO-очереди */
    else             level->head = node;
    level->tail = node;
    level->count++;
    pq->size++;
    return PQ_OK;
}

PqStatus pq_pop(PriorityQueue *pq, PqItem *out)
{
    if (!out) return PQ_ERR_ARGUMENT;

    int p = highest_level(pq);
    if (p < 0) return PQ_NOT_FOUND;

    take_head(pq, p, out);
    return PQ_OK;
}

PqStatus pq_pop_priority(PriorityQueue *pq, int priority, PqItem *out)
{
    if (!out) return PQ_ERR_ARGUMENT;
    if (!valid_priority(priority)) return PQ_ERR_PRIORITY;
    if (pq->levels[priority].count == 0) return PQ_NOT_FOUND;

    take_head(pq, priority, out);
    return PQ_OK;
}

PqStatus pq_pop_at_least(PriorityQueue *pq, int min_priority, PqItem *out)
{
    if (!out) return PQ_ERR_ARGUMENT;
    if (!valid_priority(min_priority)) return PQ_ERR_PRIORITY;

    int p = first_highest_level_from_min(pq, min_priority);
    if (p < min_priority) return PQ_NOT_FOUND;      /* включает и пустую очередь (p == -1) */

    take_head(pq, p, out);
    return PQ_OK;
}

PqStatus pq_peek(const PriorityQueue *pq, PqItem *out)
{
    if (!out) return PQ_ERR_ARGUMENT;

    int p = highest_level(pq);
    if (p < 0) return PQ_NOT_FOUND;

    out->priority = p;
    out->data = pq->levels[p].head->data;
    return PQ_OK;
}

size_t pq_size(const PriorityQueue *pq)
{
    return pq->size;
}

size_t pq_count_priority(const PriorityQueue *pq, int priority)
{
    return valid_priority(priority) ? pq->levels[priority].count : 0;
}

void pq_for_each(const PriorityQueue *pq, PqVisitor visit, void *ctx)
{
    for (int p = PQ_MAX_PRIORITY; p >= PQ_MIN_PRIORITY; p--)
        for (const PqNode *n = pq->levels[p].head; n; n = n->next)
            visit(p, n->data, ctx);
}

void pq_clear(PriorityQueue *pq, void (*destroy)(void *))
{
    for (int p = 0; p < PQ_LEVELS; p++) {
        PqNode *n = pq->levels[p].head;
        while (n) {
            PqNode *next = n->next;
            if (destroy) destroy(n->data);
            free(n);
            n = next;
        }
    }
    pq_init(pq);
}

const char *pq_status_message(PqStatus status)
{
    switch (status) {
    case PQ_OK:           return "OK";
    case PQ_NOT_FOUND:    return "no matching element";
    case PQ_ERR_PRIORITY: return "priority must be in the range 0..255";
    case PQ_ERR_MEMORY:   return "out of memory";
    case PQ_ERR_ARGUMENT: return "invalid argument";
    }
    return "unknown error";
}
