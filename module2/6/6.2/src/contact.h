#ifndef CONTACT_H
#define CONTACT_H

#include <stddef.h>

/* Динамический массив строк (телефоны, e-mail, ссылки, мессенджеры) */
typedef struct {
    char **data;
    size_t count;
} StrList;

typedef struct {
    char *name;        /* обязательное */
    char *surname;     /* обязательное */
    char *patronymic;  /* необязательные поля: пустая строка "" = не задано */
    char *work_place;
    char *post;
    StrList numbers;
    StrList emails;
    StrList links;
    StrList messengers;
} Contact;

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

/* ---- Функции для библиотеки dlist (сигнатуры DListCompare и DListDestroy) ---- */

/* Порядок контактов: фамилия, имя, отчество (без учёта регистра ASCII-букв) */
int  contact_compare(const void *a, const void *b);
/* Освобождает контакт, лежащий в списке */
void contact_destroy(void *contact);

#endif /* CONTACT_H */
