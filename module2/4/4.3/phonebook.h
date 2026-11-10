#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <stddef.h>
#include "contact.h"

/* Через сколько изменений дерева (добавление, удаление, перемещение контакта
   после смены ключа) оно балансируется автоматически. */
#define PB_DEFAULT_BALANCE_PERIOD 5

/* Узел бинарного дерева поиска. Ключ - фамилия, имя, отчество (contact_compare).
   Меньшие ключи - в левом поддереве, большие и равные - в правом. */
typedef struct TreeNode {
    Contact *contact;
    struct TreeNode *left;
    struct TreeNode *right;
    struct TreeNode *parent;
} TreeNode;

/* Телефонная книга: бинарное дерево поиска */
typedef struct {
    TreeNode *root;
    size_t count;

    size_t balance_period;           /* 0 - автоматическая балансировка выключена */
    size_t changes_since_balance;    /* изменений с момента последней балансировки */
    size_t balances_done;            /* сколько раз дерево уже балансировалось */
    size_t last_height_before;       /* высота дерева до последней балансировки */
    size_t last_height_after;        /* и после неё */
} PhoneBook;

void pb_init(PhoneBook *pb);

/* Вставляет контакт на своё место в дереве. Книга забирает владение c.
   0 - ok, -1 - нет памяти (тогда c остаётся у вызывающего). */
int  pb_add(PhoneBook *pb, Contact *c);

/* Узел с номером index (с нуля) в порядке сортировки или NULL. */
TreeNode *pb_get(const PhoneBook *pb, size_t index);

/* Вызывать после изменения фамилии, имени или отчества: если контакт стал
   нарушать порядок, его узел переставляется на правильное место. */
void pb_reposition(PhoneBook *pb, TreeNode *node);

/* Удаляет узел и освобождает контакт. 0 - ok, -1 - node == NULL. */
int  pb_delete(PhoneBook *pb, TreeNode *node);

/* ---- Балансировка ---- */
/* Перестраивает дерево в идеально сбалансированное (высоты поддеревьев
   различаются не более чем на 1). Узлы не пересоздаются, только перецепляются.
   0 - ok, -1 - нет памяти (дерево остаётся прежним и корректным). */
int  pb_balance(PhoneBook *pb);

/* Период автоматической балансировки в изменениях; 0 - выключить. */
void pb_set_balance_period(PhoneBook *pb, size_t period);

/* ---- Сведения о дереве ---- */
size_t pb_height(const PhoneBook *pb);          /* число уровней; 0 для пустого дерева */
size_t pb_min_height(size_t count);             /* наименьшая возможная высота для count узлов */

/* ---- Вывод ---- */
void pb_print_all(const PhoneBook *pb);          /* по возрастанию ключа (симметричный обход) */
void pb_print_all_reverse(const PhoneBook *pb);  /* по убыванию ключа */
void pb_print_tree(const PhoneBook *pb);         /* структура дерева боком: корень слева */

void pb_free(PhoneBook *pb);

#endif /* PHONEBOOK_H */
