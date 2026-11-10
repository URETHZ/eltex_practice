#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include "plugin_api.h"
#include "registry.h"

#define BUF_SIZE 128
#define DEFAULT_PLUGIN_DIR "plugins"

/* =====================  ВВОД И ПРОВЕРКА  ===================== */

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

/* Запрашивает вещественное число, пока ввод не станет корректным. 0 - EOF.
   Десятичная запятая допускается наравне с точкой. */
static int read_double(const char *prompt, double *out)
{
    char buf[BUF_SIZE];
    for (;;) {
        if (!read_line(prompt, buf, sizeof buf)) return 0;

        for (char *p = buf; *p; p++)
            if (*p == ',') *p = '.';

        char *end;
        errno = 0;
        double v = strtod(buf, &end);
        if (buf[0] != '\0' && *end == '\0' && errno != ERANGE && isfinite(v)) {
            *out = v;
            return 1;
        }
        printf("Invalid number, try again.\n");
    }
}

/* Запрашивает целое число из диапазона [min, max]. 0 - EOF. */
static int read_choice(const char *prompt, long min, long max, long *out)
{
    char buf[BUF_SIZE];
    for (;;) {
        if (!read_line(prompt, buf, sizeof buf)) return 0;

        char *end;
        errno = 0;
        long v = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && errno != ERANGE && v >= min && v <= max) {
            *out = v;
            return 1;
        }
        printf("Enter an integer from %ld to %ld.\n", min, max);
    }
}

/* =====================  ВЫВОД ОТЧЁТА ОБ ОБНОВЛЕНИИ  ===================== */

static void print_names(const char *mark, const char *title,
                        const char list[][CALC_NAME_MAX], size_t count)
{
    if (count == 0) return;
    printf("  %s %-9s", mark, title);
    size_t shown = count < REPORT_MAX ? count : REPORT_MAX;
    for (size_t i = 0; i < shown; i++)
        printf("%s%s", i ? ", " : "", list[i]);
    if (count > shown) printf(", ... (%zu more)", count - shown);
    printf("\n");
}

static void print_report(const RefreshReport *r, const char *dir, size_t commands, int first_load)
{
    if (first_load) {
        printf("Loaded %zu command(s) from '%s' (%zu library file(s) found).\n",
               commands, dir, r->scanned);
    } else if (r->added_count + r->removed_count + r->updated_count == 0) {
        printf("Menu is up to date: %zu command(s), no changes.\n", commands);
    } else {
        printf("Menu updated: %zu command(s).\n", commands);
        print_names("+", "added:", r->added, r->added_count);
        print_names("-", "removed:", r->removed, r->removed_count);
        print_names("~", "changed:", r->updated, r->updated_count);
    }

    size_t shown = r->skipped_count < REPORT_MAX ? r->skipped_count : REPORT_MAX;
    for (size_t i = 0; i < shown; i++)
        printf("  ! skipped %s\n", r->skipped[i]);
    if (r->skipped_count > shown)
        printf("  ! ... and %zu more skipped\n", r->skipped_count - shown);
}

/* Перечитывает каталог и сообщает, что изменилось. 0 - только если ввод не нужен. */
static void refresh_menu(Registry *reg, const char *dir, int first_load)
{
    RefreshReport rep;
    int rc = registry_refresh(reg, dir, &rep);

    if (rc == -1) {
        printf("Cannot read the plugin directory '%s'.%s\n", dir,
               first_load ? " The menu is empty." : " The menu is left unchanged.");
    } else if (rc == -2) {
        printf("Out of memory. The menu is left unchanged.\n");
    } else {
        print_report(&rep, dir, reg->count, first_load);
    }
}

/* =====================  ВЫПОЛНЕНИЕ КОМАНДЫ  ===================== */

/* Запрашивает аргументы, вызывает функцию из библиотеки и печатает результат. 0 - EOF. */
static int run_command(const Command *cmd)
{
    double args[CALC_MAX_ARGS];
    size_t argc = cmd->info.arity;

    if (argc == CALC_ARITY_VARIABLE) {
        long n;
        if (!read_choice("How many numbers: ", 1, CALC_MAX_ARGS, &n)) return 0;
        argc = (size_t)n;
    }

    for (size_t i = 0; i < argc; i++) {
        char prompt[32];
        snprintf(prompt, sizeof prompt, "Argument %zu: ", i + 1);
        if (!read_double(prompt, &args[i])) return 0;
    }

    double result = 0.0;
    CalcStatus status = CALC_OK;
    char detail[160];
    LoadStatus ls = registry_execute(cmd, args, argc, &result, &status, detail, sizeof detail);

    if (ls != LOAD_OK) {
        /* Библиотеку удалили или заменили после построения меню: программа не падает */
        printf("Error: %s.\n", load_status_message(ls));
        if (detail[0]) printf("  (%s)\n", detail);
        printf("  Library: %s\n  Choose \"Refresh menu\" to update the command list.\n", cmd->path);
        return 1;
    }
    if (status != CALC_OK) {
        printf("Error: %s\n", calc_status_message(status));
        return 1;
    }

    printf("%s(", cmd->info.name);
    for (size_t i = 0; i < argc; i++)
        printf("%s%.10g", i ? ", " : "", args[i]);
    printf(") = %.10g\n", result);
    return 1;
}

/* =====================  main  ===================== */

int main(int argc, char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [plugin_directory]   (default: %s)\n", argv[0], DEFAULT_PLUGIN_DIR);
        return 1;
    }
    const char *dir = (argc == 2) ? argv[1] : DEFAULT_PLUGIN_DIR;

    Registry reg;
    registry_init(&reg);
    refresh_menu(&reg, dir, 1);                  /* при запуске считываем каталог */

    for (;;) {
        printf("\n=== Calculator ===\n");
        for (size_t i = 0; i < reg.count; i++)
            printf("%zu) %s\n", i + 1, reg.items[i].info.description);
        printf("%zu) Refresh menu\n0) Exit\n", reg.count + 1);

        long choice;
        if (!read_choice("Action: ", 0, (long)reg.count + 1, &choice) || choice == 0) break;

        if ((size_t)choice == reg.count + 1) {
            refresh_menu(&reg, dir, 0);
        } else if (!run_command(&reg.items[choice - 1])) {
            break;
        }
    }

    printf("Bye.\n");
    registry_free(&reg);
    return 0;
}
