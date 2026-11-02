#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "contact.h"

/* ---------- внутренние помощники ---------- */

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Заменяет *field копией value. При ошибке старое значение сохраняется. */
static int set_str(char **field, const char *value)
{
    char *copy = dup_str(value);
    if (!copy) return -1;
    free(*field);
    *field = copy;
    return 0;
}

static void strlist_free(StrList *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->data[i]);
    free(list->data);
    list->data = NULL;
    list->count = 0;
}

static void print_list(const char *label, const StrList *list)
{
    printf("%s: ", label);
    if (list->count == 0) {
        printf("none\n");
        return;
    }
    for (size_t i = 0; i < list->count; i++)
        printf("%s%s", i ? ", " : "", list->data[i]);
    printf("\n");
}

static const char *or_none(const char *s)
{
    return (s && s[0] != '\0') ? s : "none";
}

/* ---------- StrList ---------- */

int strlist_add(StrList *list, const char *value)
{
    char *copy = dup_str(value);
    if (!copy) return -1;

    char **tmp = (char **)realloc(list->data, (list->count + 1) * sizeof(char *));
    if (!tmp) {
        free(copy);
        return -1;
    }
    list->data = tmp;
    list->data[list->count++] = copy;
    return 0;
}

int strlist_remove(StrList *list, size_t index)
{
    if (index >= list->count) return -1;

    free(list->data[index]);
    memmove(&list->data[index], &list->data[index + 1],
            (list->count - index - 1) * sizeof(char *));
    list->count--;

    if (list->count == 0) {
        free(list->data);
        list->data = NULL;
    }
    return 0;
}

/* ---------- Contact ---------- */

Contact *contact_create(const char *name, const char *surname)
{
    if (!name || !surname || name[0] == '\0' || surname[0] == '\0')
        return NULL;

    Contact *c = (Contact *)calloc(1, sizeof(Contact));
    if (!c) return NULL;

    if (set_str(&c->name, name) || set_str(&c->surname, surname) ||
        set_str(&c->patronymic, "") || set_str(&c->work_place, "") ||
        set_str(&c->post, "")) {
        contact_free(c);
        return NULL;
    }
    return c;
}

int contact_set_name(Contact *c, const char *value)
{
    if (!c || !value || value[0] == '\0') return -1;
    return set_str(&c->name, value);
}

int contact_set_surname(Contact *c, const char *value)
{
    if (!c || !value || value[0] == '\0') return -1;
    return set_str(&c->surname, value);
}

int contact_set_patronymic(Contact *c, const char *value)
{
    return c && value ? set_str(&c->patronymic, value) : -1;
}

int contact_set_work_place(Contact *c, const char *value)
{
    return c && value ? set_str(&c->work_place, value) : -1;
}

int contact_set_post(Contact *c, const char *value)
{
    return c && value ? set_str(&c->post, value) : -1;
}

void contact_print(const Contact *c)
{
    printf("Surname: %s\n", c->surname);
    printf("Name: %s\n", c->name);
    printf("Patronymic: %s\n", or_none(c->patronymic));
    printf("Work place: %s\n", or_none(c->work_place));
    printf("Post: %s\n", or_none(c->post));
    print_list("Numbers", &c->numbers);
    print_list("Emails", &c->emails);
    print_list("Links", &c->links);
    print_list("Messengers", &c->messengers);
}

void contact_free(Contact *c)
{
    if (!c) return;
    free(c->name);
    free(c->surname);
    free(c->patronymic);
    free(c->work_place);
    free(c->post);
    strlist_free(&c->numbers);
    strlist_free(&c->emails);
    strlist_free(&c->links);
    strlist_free(&c->messengers);
    free(c);
}

/* ---------- упорядочивание ---------- */

/* Сравнение строк без учёта регистра ASCII-букв.
   (Кириллица в UTF-8 сравнивается побайтно, с учётом регистра.) */
static int compare_ci(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int contact_compare(const Contact *a, const Contact *b)
{
    int r = compare_ci(a->surname, b->surname);
    if (r != 0) return r;
    r = compare_ci(a->name, b->name);
    if (r != 0) return r;
    return compare_ci(a->patronymic, b->patronymic);
}

/* ---------- двухсвязный список ---------- */

/* Вынимает узел из списка, не освобождая его */
static void unlink_node(PhoneBook *pb, ContactNode *n)
{
    if (n->prev) n->prev->next = n->next;
    else         pb->head = n->next;

    if (n->next) n->next->prev = n->prev;
    else         pb->tail = n->prev;

    n->prev = NULL;
    n->next = NULL;
    pb->count--;
}

/* Вставляет узел на своё место. Равные по ключу контакты идут в порядке добавления. */
static void link_sorted(PhoneBook *pb, ContactNode *n)
{
    if (!pb->tail || contact_compare(n->contact, pb->tail->contact) >= 0) {
        /* самый частый случай: новый контакт не меньше последнего - в конец за O(1) */
        n->prev = pb->tail;
        n->next = NULL;
        if (pb->tail) pb->tail->next = n;
        else          pb->head = n;
        pb->tail = n;
    } else {
        /* новый контакт строго меньше последнего, значит, цикл гарантированно остановится */
        ContactNode *cur = pb->head;
        while (contact_compare(n->contact, cur->contact) >= 0)
            cur = cur->next;

        n->next = cur;                    /* вставка перед cur */
        n->prev = cur->prev;
        if (cur->prev) cur->prev->next = n;
        else           pb->head = n;
        cur->prev = n;
    }
    pb->count++;
}

void pb_init(PhoneBook *pb)
{
    pb->head = NULL;
    pb->tail = NULL;
    pb->count = 0;
}

int pb_add(PhoneBook *pb, Contact *c)
{
    if (!c) return -1;

    ContactNode *n = (ContactNode *)calloc(1, sizeof(ContactNode));
    if (!n) return -1;

    n->contact = c;
    link_sorted(pb, n);
    return 0;
}

ContactNode *pb_get(const PhoneBook *pb, size_t index)
{
    if (index >= pb->count) return NULL;

    ContactNode *n;
    if (index < pb->count / 2) {              /* ближе к началу - идём вперёд */
        n = pb->head;
        for (size_t i = 0; i < index; i++) n = n->next;
    } else {                                  /* ближе к концу - идём назад */
        n = pb->tail;
        for (size_t i = pb->count - 1; i > index; i--) n = n->prev;
    }
    return n;
}

void pb_reposition(PhoneBook *pb, ContactNode *n)
{
    if (!n) return;

    int in_place = (!n->prev || contact_compare(n->prev->contact, n->contact) <= 0) &&
                   (!n->next || contact_compare(n->contact, n->next->contact) <= 0);
    if (in_place) return;

    unlink_node(pb, n);
    link_sorted(pb, n);
}

int pb_delete(PhoneBook *pb, ContactNode *n)
{
    if (!n) return -1;

    unlink_node(pb, n);
    contact_free(n->contact);
    free(n);
    return 0;
}

void pb_print_all(const PhoneBook *pb)
{
    if (pb->count == 0) {
        printf("Phonebook is empty.\n");
        return;
    }
    size_t i = 1;
    for (const ContactNode *n = pb->head; n; n = n->next, i++) {
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
    size_t i = pb->count;                     /* номера те же, что и при прямом обходе */
    for (const ContactNode *n = pb->tail; n; n = n->prev, i--) {
        printf("[%zu]\n", i);
        contact_print(n->contact);
        printf("---------------\n");
    }
}

void pb_free(PhoneBook *pb)
{
    ContactNode *n = pb->head;
    while (n) {
        ContactNode *next = n->next;
        contact_free(n->contact);
        free(n);
        n = next;
    }
    pb_init(pb);
}
