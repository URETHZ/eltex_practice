#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "contact.h"

#define BUF_SIZE 256

typedef int (*Validator)(const char *);

/* =====================  ВВОД  ===================== */

/* Читает строку, убирает пробелы по краям. 0 - конец ввода (EOF). */
static int read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin)) return 0;

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        len--;
    } else {                       /* строка не поместилась - сбрасываем остаток */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { }
    }
    buf[len] = '\0';

    size_t start = 0;
    while (buf[start] && isspace((unsigned char)buf[start])) start++;
    while (len > start && isspace((unsigned char)buf[len - 1])) len--;
    memmove(buf, buf + start, len - start);
    buf[len - start] = '\0';
    return 1;
}

/* Читает целое число из диапазона [min, max]. 0 - EOF. */
static int read_number(const char *prompt, long min, long max, long *out)
{
    char buf[BUF_SIZE];
    for (;;) {
        if (!read_line(prompt, buf, sizeof buf)) return 0;
        char *end;
        long v = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && v >= min && v <= max) {
            *out = v;
            return 1;
        }
        printf("Enter a number from %ld to %ld.\n", min, max);
    }
}

/* =====================  ВАЛИДАЦИЯ  ===================== */

static int is_nonempty(const char *s)
{
    return s[0] != '\0';
}

static int is_phone(const char *s)
{
    int digits = 0;
    for (size_t i = 0; s[i]; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (isdigit(ch)) digits++;
        else if (ch == '+' && i == 0) continue;
        else if (ch == ' ' || ch == '-' || ch == '(' || ch == ')') continue;
        else return 0;
    }
    return digits >= 5 && digits <= 15;
}

static int is_email(const char *s)
{
    const char *at = strchr(s, '@');
    if (!at || at == s || strchr(at + 1, '@')) return 0;
    if (strchr(s, ' ')) return 0;
    const char *dot = strrchr(at, '.');
    return dot && dot > at + 1 && dot[1] != '\0';
}

/* Запрашивает значение, пока оно не пройдёт проверку. 0 - EOF. */
static int ask_valid(const char *prompt, char *buf, size_t size,
                     Validator check, const char *error_msg)
{
    for (;;) {
        if (!read_line(prompt, buf, size)) return 0;
        if (check(buf)) return 1;
        printf("%s\n", error_msg);
    }
}

/* =====================  РАБОТА СО СПИСКАМИ  ===================== */

static void print_numbered(const StrList *list)
{
    if (list->count == 0) {
        printf("  (empty)\n");
        return;
    }
    for (size_t i = 0; i < list->count; i++)
        printf("  %zu) %s\n", i + 1, list->data[i]);
}

/* Добавление записей в список, пока не введут пустую строку. */
static void fill_list(StrList *list, const char *title, Validator check, const char *error_msg)
{
    char prompt[BUF_SIZE], buf[BUF_SIZE];
    snprintf(prompt, sizeof prompt, "%s (empty line - finish): ", title);

    for (;;) {
        if (!read_line(prompt, buf, sizeof buf) || buf[0] == '\0') return;
        if (!check(buf)) {
            printf("%s\n", error_msg);
            continue;
        }
        if (strlist_add(list, buf) != 0) {
            printf("Out of memory.\n");
            return;
        }
    }
}

static void edit_list(StrList *list, const char *title, Validator check, const char *error_msg)
{
    for (;;) {
        printf("\n%s:\n", title);
        print_numbered(list);
        printf("1) Add\n2) Delete\n0) Back\n");

        long choice;
        if (!read_number("> ", 0, 2, &choice) || choice == 0) return;

        if (choice == 1) {
            char buf[BUF_SIZE];
            if (!ask_valid("New value: ", buf, sizeof buf, check, error_msg)) return;
            if (strlist_add(list, buf) != 0) printf("Out of memory.\n");
        } else {
            if (list->count == 0) {
                printf("Nothing to delete.\n");
                continue;
            }
            long idx;
            if (!read_number("Number to delete (0 - cancel): ", 0, (long)list->count, &idx)) return;
            if (idx > 0) strlist_remove(list, (size_t)idx - 1);
        }
    }
}

/* =====================  ДЕЙСТВИЯ МЕНЮ  ===================== */

static void action_add(PhoneBook *pb)
{
    char name[BUF_SIZE], surname[BUF_SIZE], buf[BUF_SIZE];

    if (!ask_valid("Surname (required): ", surname, sizeof surname, is_nonempty, "Surname is required.")) return;
    if (!ask_valid("Name (required): ", name, sizeof name, is_nonempty, "Name is required.")) return;

    Contact *c = contact_create(name, surname);
    if (!c) {
        printf("Could not create contact.\n");
        return;
    }

    if (read_line("Patronymic (Enter - skip): ", buf, sizeof buf)) contact_set_patronymic(c, buf);
    if (read_line("Work place (Enter - skip): ", buf, sizeof buf)) contact_set_work_place(c, buf);
    if (read_line("Post (Enter - skip): ", buf, sizeof buf))       contact_set_post(c, buf);

    fill_list(&c->numbers,    "Phone number", is_phone,    "Invalid phone (5-15 digits, may contain + ( ) - and spaces).");
    fill_list(&c->emails,     "E-mail",       is_email,    "Invalid e-mail.");
    fill_list(&c->links,      "Social link",  is_nonempty, "Empty value.");
    fill_list(&c->messengers, "Messenger",    is_nonempty, "Empty value.");

    if (pb_add(pb, c) != 0) {
        contact_free(c);
        printf("Out of memory.\n");
        return;
    }
    printf("Contact added.\n");
}

/* Просит выбрать контакт по номеру в списке. NULL - отмена/пустая книга. */
static ContactNode *choose_contact(const PhoneBook *pb)
{
    if (pb->count == 0) {
        printf("Phonebook is empty.\n");
        return NULL;
    }
    pb_print_all(pb);
    long idx;
    if (!read_number("Contact number (0 - cancel): ", 0, (long)pb->count, &idx) || idx == 0)
        return NULL;
    return pb_get(pb, (size_t)idx - 1);
}

static void edit_text_field(Contact *c, const char *prompt, int required,
                            int (*setter)(Contact *, const char *))
{
    char buf[BUF_SIZE];
    if (required) {
        if (!ask_valid(prompt, buf, sizeof buf, is_nonempty, "This field is required.")) return;
    } else {
        if (!read_line(prompt, buf, sizeof buf)) return;   /* пустая строка очищает поле */
    }
    if (setter(c, buf) != 0) printf("Could not change the field.\n");
}

static void action_edit(PhoneBook *pb)
{
    ContactNode *node = choose_contact(pb);
    if (!node) return;
    Contact *c = node->contact;

    for (;;) {
        printf("\n");
        contact_print(c);
        printf("\n1) Surname\n2) Name\n3) Patronymic\n4) Work place\n5) Post\n"
               "6) Numbers\n7) Emails\n8) Links\n9) Messengers\n0) Back\n");

        long choice;
        if (!read_number("> ", 0, 9, &choice) || choice == 0) return;

        switch (choice) {
        case 1: edit_text_field(c, "New surname: ", 1, contact_set_surname); break;
        case 2: edit_text_field(c, "New name: ", 1, contact_set_name); break;
        case 3: edit_text_field(c, "New patronymic (empty - clear): ", 0, contact_set_patronymic); break;
        case 4: edit_text_field(c, "New work place (empty - clear): ", 0, contact_set_work_place); break;
        case 5: edit_text_field(c, "New post (empty - clear): ", 0, contact_set_post); break;
        case 6: edit_list(&c->numbers, "Numbers", is_phone, "Invalid phone (5-15 digits)."); break;
        case 7: edit_list(&c->emails, "Emails", is_email, "Invalid e-mail."); break;
        case 8: edit_list(&c->links, "Links", is_nonempty, "Empty value."); break;
        case 9: edit_list(&c->messengers, "Messengers", is_nonempty, "Empty value."); break;
        }

        /* Фамилия, имя и отчество - ключ сортировки: после их изменения
           контакт может переехать на другое место в списке. */
        if (choice <= 3) pb_reposition(pb, node);
    }
}

static void action_delete(PhoneBook *pb)
{
    ContactNode *node = choose_contact(pb);
    if (!node) return;
    Contact *c = node->contact;

    char buf[BUF_SIZE];
    printf("Delete %s %s? ", c->surname, c->name);
    if (!read_line("(y/n): ", buf, sizeof buf)) return;

    if (buf[0] == 'y' || buf[0] == 'Y') {
        pb_delete(pb, node);
        printf("Contact deleted.\n");
    } else {
        printf("Cancelled.\n");
    }
}

/* =====================  main  ===================== */

int main(void)
{
    PhoneBook pb;
    pb_init(&pb);

    for (;;) {
        printf("\n=== Phonebook ===\n"
               "1) Add contact\n2) Edit contact\n3) Delete contact\n"
               "4) Show all (sorted)\n5) Show all in reverse order\n0) Exit\n");

        long choice;
        if (!read_number("> ", 0, 5, &choice) || choice == 0) break;

        switch (choice) {
        case 1: action_add(&pb); break;
        case 2: action_edit(&pb); break;
        case 3: action_delete(&pb); break;
        case 4: pb_print_all(&pb); break;
        case 5: pb_print_all_reverse(&pb); break;
        }
    }

    pb_free(&pb);
    return 0;
}
