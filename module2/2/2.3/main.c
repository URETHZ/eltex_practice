#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include "operations.h"
#include "registry.h"

#define BUF_SIZE 128

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

/* Таблица встроенных команд. Меню строится из реестра, а не из этой таблицы:
   чтобы добавить команду, достаточно написать функцию и добавить строку сюда. */
static const struct {
    const char *name;
    const char *description;
    size_t arity;
    CalcFunc func;
} DEFAULT_COMMANDS[] = {
    { "add",  "Addition        (a + b)",           2, op_add },
    { "sub",  "Subtraction     (a - b)",           2, op_sub },
    { "mul",  "Multiplication  (a * b)",           2, op_mul },
    { "div",  "Division        (a / b)",           2, op_div },
    { "pow",  "Power           (a ^ b)",           2, op_pow },
    { "sqrt", "Square root",                       1, op_sqrt },
    { "fact", "Factorial       (n!, integer)",     1, op_factorial },
    { "mod",  "Remainder       (a % b, integers)", 2, op_mod },
    { "sum",  "Sum of several numbers",            ARITY_VARIABLE, op_sum },
    { "mean", "Arithmetic mean of several numbers", ARITY_VARIABLE, op_mean },
    { "max",  "Maximum of several numbers",        ARITY_VARIABLE, op_max },
};

static int register_default_commands(Registry *reg)
{
    size_t n = sizeof DEFAULT_COMMANDS / sizeof DEFAULT_COMMANDS[0];
    for (size_t i = 0; i < n; i++) {
        if (registry_add(reg, DEFAULT_COMMANDS[i].name, DEFAULT_COMMANDS[i].description,
                         DEFAULT_COMMANDS[i].arity, DEFAULT_COMMANDS[i].func) != 0)
            return -1;
    }
    return 0;
}

/* =====================  ВЫПОЛНЕНИЕ КОМАНДЫ  ===================== */

/* Запрашивает аргументы, вызывает команду и печатает результат. 0 - EOF. */
static int run_command(const Command *cmd)
{
    double args[MAX_ARGS];
    size_t argc = cmd->arity;

    if (argc == ARITY_VARIABLE) {
        long n;
        if (!read_choice("How many numbers: ", 1, MAX_ARGS, &n)) return 0;
        argc = (size_t)n;
    }

    for (size_t i = 0; i < argc; i++) {
        char prompt[32];
        snprintf(prompt, sizeof prompt, "Argument %zu: ", i + 1);
        if (!read_double(prompt, &args[i])) return 0;
    }

    double result;
    CalcStatus st = registry_execute(cmd, args, argc, &result);
    if (st != CALC_OK) {
        printf("Error: %s\n", calc_status_message(st));
        return 1;
    }

    printf("%s(", cmd->name);
    for (size_t i = 0; i < argc; i++)
        printf("%s%.10g", i ? ", " : "", args[i]);
    printf(") = %.10g\n", result);
    return 1;
}

/* Меню включения/выключения команд. 0 - EOF. */
static int manage_commands(Registry *reg)
{
    for (;;) {
        printf("\n--- Manage commands ---\n");
        for (size_t i = 0; i < reg->count; i++) {
            const Command *c = registry_get(reg, i);
            printf("%zu) [%s] %s\n", i + 1, c->enabled ? "on " : "off", c->description);
        }

        long choice;
        if (!read_choice("Number to toggle (0 - back): ", 0, (long)reg->count, &choice)) return 0;
        if (choice == 0) return 1;

        const Command *c = registry_get(reg, (size_t)choice - 1);
        registry_set_enabled(reg, (size_t)choice - 1, !c->enabled);
    }
}

int main(void)
{
    #ifdef _WIN32
        system("chcp 65001 > nul");
    #endif
    Registry reg;
    registry_init(&reg);

    if (register_default_commands(&reg) != 0) {
        fprintf(stderr, "Failed to register commands.\n");
        registry_free(&reg);
        return 1;
    }

    for (;;) {
        /* Меню строится динамически из включённых команд реестра */
        size_t enabled = registry_enabled_count(&reg);

        printf("\n=== Calculator ===\n");
        for (size_t k = 0; k < enabled; k++)
            printf("%zu) %s\n", k + 1, registry_nth_enabled(&reg, k)->description);
        printf("%zu) Manage commands\n0) Exit\n", enabled + 1);

        long choice;
        if (!read_choice("Action: ", 0, (long)enabled + 1, &choice) || choice == 0) break;

        if ((size_t)choice == enabled + 1) {
            if (!manage_commands(&reg)) break;
        } else {
            const Command *cmd = registry_nth_enabled(&reg, (size_t)choice - 1);
            if (!run_command(cmd)) break;
        }
    }

    printf("Bye.\n");
    registry_free(&reg);
    return 0;
}
