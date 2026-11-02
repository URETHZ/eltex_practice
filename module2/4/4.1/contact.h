#ifndef CONTACT_H
#define CONTACT_H

#include <stddef.h>

/* телефоны, e-mail, ссылки, мессенджеры */
typedef struct {
    char **data;
    size_t count;
} StrList;

typedef struct {
    char *name;        
    char *surname;     
    char *patronymic;  /* строка "" = не задано */
    char *work_place;
    char *post;
    StrList numbers;
    StrList emails;
    StrList links;
    StrList messengers;
} Contact;

/* Узел двухсвязного списка */
typedef struct ContactNode {
    Contact *contact;
    struct ContactNode *prev;
    struct ContactNode *next;
} ContactNode;

/* Телефонная книга: двухсвязный список, упорядоченный по
   фамилии, имени и отчеству. */
typedef struct {
    ContactNode *head;     /* первый (наименьший) контакт */
    ContactNode *tail;     /* последний (наибольший) контакт */
    size_t count;
} PhoneBook;

/* ---- StrList ---- */
int  strlist_add(StrList *list, const char *value);      /* 0 - ok, -1 - ошибка */
int  strlist_remove(StrList *list, size_t index);        /* 0 - ok, -1 - плохой индекс */

/* ---- Contact ---- */
Contact *contact_create(const char *name, const char *surname); /* NULL при ошибке/пустых name, surname */
int  contact_set_name(Contact *c, const char *value);       /* пустое значение -> -1 */
int  contact_set_surname(Contact *c, const char *value);    /* пустое значение -> -1 */
int  contact_set_patronymic(Contact *c, const char *value);
int  contact_set_work_place(Contact *c, const char *value);
int  contact_set_post(Contact *c, const char *value);
void contact_print(const Contact *c);
void contact_free(Contact *c);

/* Сравнение по ключу упорядочивания: фамилия, имя, отчество.
   <0 - a раньше b, 0 - равны, >0 - a позже b. */
int  contact_compare(const Contact *a, const Contact *b);

/* ---- PhoneBook (двухсвязный упорядоченный список) ---- */
void pb_init(PhoneBook *pb);

/* Вставляет контакт на своё место в порядке сортировки. Книга забирает владение c.
   0 - ok, -1 - нет памяти (тогда c остаётся у вызывающего). */
int  pb_add(PhoneBook *pb, Contact *c);

/* Узел с номером index (с нуля) или NULL. Идёт с ближайшего к нему конца списка. */
ContactNode *pb_get(const PhoneBook *pb, size_t index);

/* Вызывать после изменения фамилии, имени или отчества: если порядок нарушен,
   узел переставляется на правильное место (без выделения памяти). */
void pb_reposition(PhoneBook *pb, ContactNode *node);

/* Удаляет узел и освобождает контакт. 0 - ok, -1 - node == NULL. */
int  pb_delete(PhoneBook *pb, ContactNode *node);

void pb_print_all(const PhoneBook *pb);           /* от начала к концу */
void pb_print_all_reverse(const PhoneBook *pb);   /* от конца к началу (через prev) */
void pb_free(PhoneBook *pb);

#endif /* CONTACT_H */
