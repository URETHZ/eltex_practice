#include <stdio.h>
#include <stdlib.h>
#include "phonebook.h"

#define MAX_DRAWN_NODES 64      /* больше узлов в виде дерева не рисуем */

/* ---------- обход по родительским ссылкам (без рекурсии) ---------- */

static TreeNode *tree_min(TreeNode *n)
{
    while (n->left) n = n->left;
    return n;
}

static TreeNode *tree_max(TreeNode *n)
{
    while (n->right) n = n->right;
    return n;
}

/* Следующий узел в порядке возрастания ключа или NULL */
static TreeNode *successor(TreeNode *n)
{
    if (n->right) return tree_min(n->right);
    TreeNode *p = n->parent;
    while (p && n == p->right) {
        n = p;
        p = p->parent;
    }
    return p;
}

/* Предыдущий узел в порядке возрастания ключа или NULL */
static TreeNode *predecessor(TreeNode *n)
{
    if (n->left) return tree_max(n->left);
    TreeNode *p = n->parent;
    while (p && n == p->left) {
        n = p;
        p = p->parent;
    }
    return p;
}

/* ---------- вставка и удаление узла ---------- */

/* Вставляет готовый узел в дерево. Равные ключи уходят вправо. */
static void attach_node(PhoneBook *pb, TreeNode *n)
{
    TreeNode *parent = NULL;
    TreeNode *cur = pb->root;
    int go_left = 0;

    while (cur) {
        parent = cur;
        go_left = contact_compare(n->contact, cur->contact) < 0;
        cur = go_left ? cur->left : cur->right;
    }

    n->left = NULL;
    n->right = NULL;
    n->parent = parent;
    if (!parent)       pb->root = n;
    else if (go_left)  parent->left = n;
    else               parent->right = n;
    pb->count++;
}

/* Заменяет в дереве поддерево с корнем u поддеревом с корнем v */
static void transplant(PhoneBook *pb, TreeNode *u, TreeNode *v)
{
    if (!u->parent)             pb->root = v;
    else if (u == u->parent->left) u->parent->left = v;
    else                        u->parent->right = v;
    if (v) v->parent = u->parent;
}

/* Вынимает узел из дерева, не освобождая его (контакт остаётся в узле).
   Остальные узлы только перецепляются, их адреса не меняются. */
static void detach_node(PhoneBook *pb, TreeNode *z)
{
    if (!z->left) {
        transplant(pb, z, z->right);
    } else if (!z->right) {
        transplant(pb, z, z->left);
    } else {
        TreeNode *y = tree_min(z->right);         /* преемник z */
        if (y->parent != z) {
            transplant(pb, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(pb, z, y);
        y->left = z->left;
        y->left->parent = y;
    }
    z->left = NULL;
    z->right = NULL;
    z->parent = NULL;
    pb->count--;
}

/* Учитывает изменение дерева и по истечении периода запускает балансировку */
static void note_change(PhoneBook *pb)
{
    pb->changes_since_balance++;
    if (pb->balance_period > 0 && pb->changes_since_balance >= pb->balance_period)
        pb_balance(pb);
}

/* ---------- балансировка ---------- */

/* Собирает из отсортированного массива a[lo..hi) идеально сбалансированное
   поддерево: средний элемент становится корнем, левая и правая половины -
   его поддеревьями. Глубина рекурсии - порядка log2(n). */
static TreeNode *build_balanced(TreeNode **a, size_t lo, size_t hi, TreeNode *parent)
{
    if (lo >= hi) return NULL;

    size_t mid = lo + (hi - lo) / 2;
    TreeNode *n = a[mid];
    n->parent = parent;
    n->left = build_balanced(a, lo, mid, n);
    n->right = build_balanced(a, mid + 1, hi, n);
    return n;
}

int pb_balance(PhoneBook *pb)
{
    size_t before = pb_height(pb);

    if (pb->count > 0) {
        TreeNode **nodes = (TreeNode **)malloc(pb->count * sizeof(TreeNode *));
        if (!nodes) return -1;                    /* дерево не тронуто */

        size_t i = 0;                             /* узлы в порядке возрастания ключа */
        for (TreeNode *n = tree_min(pb->root); n; n = successor(n))
            nodes[i++] = n;

        pb->root = build_balanced(nodes, 0, pb->count, NULL);
        free(nodes);
    }

    pb->changes_since_balance = 0;
    pb->balances_done++;
    pb->last_height_before = before;
    pb->last_height_after = pb_height(pb);
    return 0;
}

void pb_set_balance_period(PhoneBook *pb, size_t period)
{
    pb->balance_period = period;
}

/* ---------- публичные операции ---------- */

void pb_init(PhoneBook *pb)
{
    pb->root = NULL;
    pb->count = 0;
    pb->balance_period = PB_DEFAULT_BALANCE_PERIOD;
    pb->changes_since_balance = 0;
    pb->balances_done = 0;
    pb->last_height_before = 0;
    pb->last_height_after = 0;
}

int pb_add(PhoneBook *pb, Contact *c)
{
    if (!c) return -1;

    TreeNode *n = (TreeNode *)calloc(1, sizeof(TreeNode));
    if (!n) return -1;

    n->contact = c;
    attach_node(pb, n);
    note_change(pb);
    return 0;
}

TreeNode *pb_get(const PhoneBook *pb, size_t index)
{
    if (index >= pb->count) return NULL;

    TreeNode *n;
    if (index < pb->count / 2) {                  /* ближе к наименьшему - идём вперёд */
        n = tree_min(pb->root);
        for (size_t i = 0; i < index; i++) n = successor(n);
    } else {                                      /* ближе к наибольшему - идём назад */
        n = tree_max(pb->root);
        for (size_t i = pb->count - 1; i > index; i--) n = predecessor(n);
    }
    return n;
}

void pb_reposition(PhoneBook *pb, TreeNode *n)
{
    if (!n) return;

    /* Структура дерева ещё старая, поэтому соседи по порядку определяются верно.
       Если ключ по-прежнему лежит между ними, дерево остаётся корректным. */
    TreeNode *prev = predecessor(n);
    TreeNode *next = successor(n);
    int in_place = (!prev || contact_compare(prev->contact, n->contact) <= 0) &&
                   (!next || contact_compare(n->contact, next->contact) <= 0);
    if (in_place) return;

    detach_node(pb, n);
    attach_node(pb, n);
    note_change(pb);
}

int pb_delete(PhoneBook *pb, TreeNode *n)
{
    if (!n) return -1;

    detach_node(pb, n);
    contact_free(n->contact);
    free(n);
    note_change(pb);
    return 0;
}

/* ---------- сведения о дереве ---------- */

size_t pb_height(const PhoneBook *pb)
{
    /* Обход без рекурсии: идём по дереву, следя за глубиной и за тем,
       откуда пришли (от родителя, из левого или из правого потомка). */
    TreeNode *n = pb->root;
    TreeNode *prev = NULL;
    size_t depth = 1, best = 0;

    while (n) {
        if (depth > best) best = depth;

        TreeNode *next;
        if (prev == n->parent) {                  /* пришли сверху */
            if (n->left)       { next = n->left;  depth++; }
            else if (n->right) { next = n->right; depth++; }
            else               { next = n->parent; depth--; }
        } else if (prev == n->left) {             /* вернулись из левого поддерева */
            if (n->right)      { next = n->right; depth++; }
            else               { next = n->parent; depth--; }
        } else {                                  /* вернулись из правого поддерева */
            next = n->parent;
            depth--;
        }
        prev = n;
        n = next;
    }
    return best;
}

size_t pb_min_height(size_t count)
{
    size_t capacity = 0, height = 0;              /* уровней height вмещают 2^height - 1 узлов */
    while (capacity < count) {
        capacity = capacity * 2 + 1;
        height++;
    }
    return height;
}

/* ---------- вывод ---------- */

void pb_print_all(const PhoneBook *pb)
{
    if (pb->count == 0) {
        printf("Phonebook is empty.\n");
        return;
    }
    size_t i = 1;
    for (TreeNode *n = tree_min(pb->root); n; n = successor(n), i++) {
        printf("[%zu]\n", i);
        contact_print(n->contact);
        printf("---------------\n");
    }
}

void pb_print_all_reverse(const PhoneBook *pb)
{
    if (pb->count == 0) {
        printf("Phonebook is empty.\n");
        return;
    }
    size_t i = pb->count;                         /* номера те же, что и при прямом обходе */
    for (TreeNode *n = tree_max(pb->root); n; n = predecessor(n), i--) {
        printf("[%zu]\n", i);
        contact_print(n->contact);
        printf("---------------\n");
    }
}

static void print_subtree(const TreeNode *n, int depth)
{
    if (!n) return;
    print_subtree(n->right, depth + 1);           /* правое поддерево - сверху */
    printf("%*s%s %s\n", depth * 4, "", n->contact->surname, n->contact->name);
    print_subtree(n->left, depth + 1);
}

void pb_print_tree(const PhoneBook *pb)
{
    if (pb->count == 0) {
        printf("Phonebook is empty.\n");
        return;
    }
    if (pb->count > MAX_DRAWN_NODES) {
        printf("Tree is too large to draw (%zu contacts, limit %d).\n", pb->count, MAX_DRAWN_NODES);
        return;
    }
    printf("(root is at the left; larger keys are drawn above, smaller below)\n");
    print_subtree(pb->root, 0);
}

/* ---------- освобождение ---------- */

void pb_free(PhoneBook *pb)
{
    /* Обход в глубину без рекурсии: спускаемся к листу, освобождаем его,
       поднимаемся к родителю. Работает и для вырожденного дерева-цепочки. */
    TreeNode *n = pb->root;
    while (n) {
        if (n->left) {
            n = n->left;
        } else if (n->right) {
            n = n->right;
        } else {
            TreeNode *p = n->parent;
            if (p) {
                if (p->left == n) p->left = NULL;
                else              p->right = NULL;
            }
            contact_free(n->contact);
            free(n);
            n = p;
        }
    }
    pb->root = NULL;
    pb->count = 0;
}
