#ifndef CONTACT_H
#define CONTACT_H

//#include <stddef.h>

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

/* Телефонная книга: динамический массив указателей на контакты */
typedef struct {
    Contact **items;
    size_t count;
    size_t capacity;
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

/* ---- PhoneBook ---- */
void pb_init(PhoneBook *pb);
int  pb_add(PhoneBook *pb, Contact *c);                  /* книга забирает владение c */
int  pb_delete(PhoneBook *pb, size_t index);             /* освобождает контакт */
Contact *pb_get(const PhoneBook *pb, size_t index);      /* NULL, если индекс неверный */
void pb_print_all(const PhoneBook *pb);
void pb_free(PhoneBook *pb);

#endif /* CONTACT_H */