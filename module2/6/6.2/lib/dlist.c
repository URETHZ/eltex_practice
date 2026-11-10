#include <stdlib.h>
#include "dlist.h"

/* Вынимает узел из списка, не освобождая его */
static void unlink_node(DList *list, DListNode *n)
{
    if (n->prev) n->prev->next = n->next;
    else         list->head = n->next;

    if (n->next) n->next->prev = n->prev;
    else         list->tail = n->prev;

    n->prev = NULL;
    n->next = NULL;
    list->count--;
}

/* Вставляет узел на своё место. Равные элементы идут в порядке добавления. */
static void link_sorted(DList *list, DListNode *n)
{
    if (!list->tail || list->compare(n->data, list->tail->data) >= 0) {
        /* самый частый случай: новый элемент не меньше последнего - в конец за O(1) */
        n->prev = list->tail;
        n->next = NULL;
        if (list->tail) list->tail->next = n;
        else            list->head = n;
        list->tail = n;
    } else {
        /* новый элемент строго меньше последнего, значит, цикл гарантированно остановится */
        DListNode *cur = list->head;
        while (list->compare(n->data, cur->data) >= 0)
            cur = cur->next;

        n->next = cur;                    /* вставка перед cur */
        n->prev = cur->prev;
        if (cur->prev) cur->prev->next = n;
        else           list->head = n;
        cur->prev = n;
    }
    list->count++;
}

void dlist_init(DList *list, DListCompare compare, DListDestroy destroy)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    list->compare = compare;
    list->destroy = destroy;
}

int dlist_insert(DList *list, void *data)
{
    if (!list || !list->compare) return -1;

    DListNode *n = (DListNode *)calloc(1, sizeof(DListNode));
    if (!n) return -1;

    n->data = data;
    link_sorted(list, n);
    return 0;
}

DListNode *dlist_get(const DList *list, size_t index)
{
    if (index >= list->count) return NULL;

    DListNode *n;
    if (index < list->count / 2) {            /* ближе к началу - идём вперёд */
        n = list->head;
        for (size_t i = 0; i < index; i++) n = n->next;
    } else {                                  /* ближе к концу - идём назад */
        n = list->tail;
        for (size_t i = list->count - 1; i > index; i--) n = n->prev;
    }
    return n;
}

void dlist_reposition(DList *list, DListNode *n)
{
    if (!n) return;

    int in_place = (!n->prev || list->compare(n->prev->data, n->data) <= 0) &&
                   (!n->next || list->compare(n->data, n->next->data) <= 0);
    if (in_place) return;

    unlink_node(list, n);
    link_sorted(list, n);
}

int dlist_remove(DList *list, DListNode *n)
{
    if (!n) return -1;

    unlink_node(list, n);
    if (list->destroy) list->destroy(n->data);
    free(n);
    return 0;
}

void dlist_for_each(const DList *list, DListVisitor visit, void *ctx)
{
    size_t position = 1;
    for (DListNode *n = list->head; n; n = n->next, position++)
        visit(n->data, position, ctx);
}

void dlist_for_each_reverse(const DList *list, DListVisitor visit, void *ctx)
{
    size_t position = list->count;            /* номера те же, что и при прямом обходе */
    for (DListNode *n = list->tail; n; n = n->prev, position--)
        visit(n->data, position, ctx);
}

void dlist_clear(DList *list)
{
    DListNode *n = list->head;
    while (n) {
        DListNode *next = n->next;
        if (list->destroy) list->destroy(n->data);
        free(n);
        n = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}
