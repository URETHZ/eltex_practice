#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* ---------- PhoneBook ---------- */

void pb_init(PhoneBook *pb)
{
    pb->items = NULL;
    pb->count = 0;
    pb->capacity = 0;
}

int pb_add(PhoneBook *pb, Contact *c)
{
    if (!c) return -1;

    if (pb->count == pb->capacity) {
        size_t new_cap = pb->capacity ? pb->capacity * 2 : 4;
        Contact **tmp = (Contact **)realloc(pb->items, new_cap * sizeof(Contact *));
        if (!tmp) return -1;
        pb->items = tmp;
        pb->capacity = new_cap;
    }
    pb->items[pb->count++] = c;
    return 0;
}

int pb_delete(PhoneBook *pb, size_t index)
{
    if (index >= pb->count) return -1;

    contact_free(pb->items[index]);
    memmove(&pb->items[index], &pb->items[index + 1],
            (pb->count - index - 1) * sizeof(Contact *));
    pb->count--;
    return 0;
}

Contact *pb_get(const PhoneBook *pb, size_t index)
{
    return index < pb->count ? pb->items[index] : NULL;
}

void pb_print_all(const PhoneBook *pb)
{
    if (pb->count == 0) {
        printf("Phonebook is empty.\n");
        return;
    }
    for (size_t i = 0; i < pb->count; i++) {
        printf("[%zu]\n", i + 1);
        contact_print(pb->items[i]);
        printf("---------------\n");
    }
}

void pb_free(PhoneBook *pb)
{
    for (size_t i = 0; i < pb->count; i++)
        contact_free(pb->items[i]);
    free(pb->items);
    pb_init(pb);
}