/* Проверка библиотеки libdlist.so отдельно от телефонной книги: в списке лежат
   обычные структуры с числами. Это показывает, что библиотека не зависит от Contact. */

#include <stdio.h>
#include <stdlib.h>
#include "dlist.h"

typedef struct {
    int key;
    int seq;         /* порядковый номер добавления - для проверки устойчивости порядка */
} Item;

static int checks = 0;

#define CHECK(cond, msg) do { checks++; if (!(cond)) { printf("FAIL: %s\n", msg); exit(1); } } while (0)

static int compare_items(const void *a, const void *b)
{
    const Item *x = (const Item *)a, *y = (const Item *)b;
    return (x->key > y->key) - (x->key < y->key);
}

static void collect(void *data, size_t position, void *ctx)
{
    Item **out = (Item **)ctx;
    out[position - 1] = (Item *)data;
}

/* Проверяет все инварианты списка */
static void check_list(const DList *list)
{
    size_t fwd = 0, bwd = 0;
    const DListNode *prev = NULL;

    for (const DListNode *n = list->head; n; prev = n, n = n->next) {
        CHECK(n->prev == prev, "prev link");
        if (prev) {
            const Item *a = (const Item *)prev->data, *b = (const Item *)n->data;
            CHECK(a->key <= b->key, "sorted");
            if (a->key == b->key) CHECK(a->seq < b->seq, "equal keys keep insertion order");
        }
        fwd++;
    }
    CHECK(prev == list->tail, "tail");
    CHECK(!list->head || !list->head->prev, "head->prev");

    const DListNode *next = NULL;
    for (const DListNode *n = list->tail; n; next = n, n = n->prev) {
        CHECK(n->next == next, "next link");
        bwd++;
    }
    CHECK(fwd == list->count && bwd == list->count, "count");

    size_t i = 0;
    for (const DListNode *n = list->head; n; n = n->next, i++)
        CHECK(dlist_get(list, i) == n, "dlist_get");
    CHECK(dlist_get(list, list->count) == NULL, "dlist_get out of range");
}

int main(void)
{
    DList list;
    dlist_init(&list, compare_items, free);

    /* 1. вставка в произвольном порядке, сразу видно порядок */
    int keys[] = { 5, 1, 4, 1, 3, 5, 2 };
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        Item *it = (Item *)malloc(sizeof(Item));
        it->key = keys[i];
        it->seq = (int)i;
        CHECK(dlist_insert(&list, it) == 0, "insert");
    }
    check_list(&list);

    Item *view[16];
    dlist_for_each(&list, collect, view);
    printf("forward :");
    for (size_t i = 0; i < list.count; i++) printf(" %d(#%d)", view[i]->key, view[i]->seq);
    printf("\n");
    dlist_for_each_reverse(&list, collect, view);
    printf("reverse :");
    for (size_t i = list.count; i > 0; i--) printf(" %d(#%d)", view[i - 1]->key, view[i - 1]->seq);
    printf("\n");

    /* 2. изменение ключа и перестановка */
    DListNode *first = dlist_get(&list, 0);
    ((Item *)first->data)->key = 100;
    dlist_reposition(&list, first);
    CHECK(list.tail == first, "moved to the end");
    check_list(&list);

    /* 3. случайные операции */
    srand(7);
    for (int step = 0; step < 20000; step++) {
        int op = rand() % 10;
        if (op < 4 || list.count == 0) {
            Item *it = (Item *)malloc(sizeof(Item));
            it->key = rand() % 20;
            it->seq = 1000 + step;
            CHECK(dlist_insert(&list, it) == 0, "insert");
        } else if (op < 7) {
            CHECK(dlist_remove(&list, dlist_get(&list, (size_t)rand() % list.count)) == 0, "remove");
        } else {
            DListNode *n = dlist_get(&list, (size_t)rand() % list.count);
            ((Item *)n->data)->key = rand() % 20;
            dlist_reposition(&list, n);
        }
        /* порядок seq среди равных ключей после reposition не гарантируется - проверяем без него */
        const DListNode *prev = NULL;
        size_t cnt = 0;
        for (const DListNode *n = list.head; n; prev = n, n = n->next, cnt++) {
            CHECK(n->prev == prev, "prev link (random)");
            if (prev) CHECK(((Item *)prev->data)->key <= ((Item *)n->data)->key, "sorted (random)");
        }
        CHECK(prev == list.tail && cnt == list.count, "tail/count (random)");
    }

    CHECK(dlist_remove(&list, NULL) == -1, "remove(NULL)");
    CHECK(dlist_insert(NULL, NULL) == -1, "insert(NULL list)");

    dlist_clear(&list);
    CHECK(list.head == NULL && list.tail == NULL && list.count == 0, "clear");

    /* после clear список пригоден для повторного использования */
    Item *again = (Item *)malloc(sizeof(Item));
    again->key = 1;
    again->seq = 0;
    CHECK(dlist_insert(&list, again) == 0, "insert after clear");
    dlist_clear(&list);

    printf("ok: %d checks passed\n", checks);
    return 0;
}
