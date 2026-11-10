#ifndef DLIST_H
#define DLIST_H

/* Библиотека libdlist.a: двухсвязный упорядоченный список.
   Библиотека ничего не знает о том, что в нём хранится: данные передаются
   как void *, а порядок задаёт функция сравнения, которую передаёт пользователь.
   Равные по сравнению элементы идут в порядке добавления. */

#include <stddef.h>

/* Узел списка */
typedef struct DListNode {
    void *data;
    struct DListNode *prev;
    struct DListNode *next;
} DListNode;

/* <0 - a раньше b, 0 - равны, >0 - a позже b */
typedef int  (*DListCompare)(const void *a, const void *b);
/* Освобождает данные элемента (может быть NULL, если данными владеет вызывающий код) */
typedef void (*DListDestroy)(void *data);
/* Вызывается для каждого элемента при обходе; position - номер элемента, начиная с 1 */
typedef void (*DListVisitor)(void *data, size_t position, void *ctx);

typedef struct {
    DListNode *head;        /* первый (наименьший) элемент */
    DListNode *tail;        /* последний (наибольший) элемент */
    size_t count;
    DListCompare compare;
    DListDestroy destroy;
} DList;

/* Подготавливает пустой список. compare обязателен, destroy может быть NULL. */
void dlist_init(DList *list, DListCompare compare, DListDestroy destroy);

/* Вставляет элемент на своё место в порядке сортировки.
   0 - ok, -1 - нет памяти или неверные аргументы (тогда data остаётся у вызывающего). */
int  dlist_insert(DList *list, void *data);

/* Узел с номером index (с нуля) или NULL. Идёт с ближайшего к нему конца списка. */
DListNode *dlist_get(const DList *list, size_t index);

/* Вызывать после изменения данных узла так, что порядок мог нарушиться:
   узел переставляется на правильное место без выделения памяти. */
void dlist_reposition(DList *list, DListNode *node);

/* Удаляет узел из списка и вызывает destroy для его данных. 0 - ok, -1 - node == NULL. */
int  dlist_remove(DList *list, DListNode *node);

/* Обход от начала к концу (position = 1..count) */
void dlist_for_each(const DList *list, DListVisitor visit, void *ctx);
/* Обход от конца к началу через ссылки prev (position = count..1) */
void dlist_for_each_reverse(const DList *list, DListVisitor visit, void *ctx);

/* Удаляет все элементы (вызывая destroy). Настройки списка сохраняются. */
void dlist_clear(DList *list);

#endif /* DLIST_H */
