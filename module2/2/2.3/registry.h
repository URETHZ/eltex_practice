#ifndef REGISTRY_H
#define REGISTRY_H

#include <stddef.h>
#include "operations.h"

#define MAX_ARGS 16          /* максимум аргументов у одной команды */
#define ARITY_VARIABLE 0     /* число аргументов вводит пользователь */

/* Одна команда калькулятора */
typedef struct {
    char *name;              /* короткое имя, уникальное в реестре ("add") */
    char *description;       /* текст пункта меню ("Addition (a + b)") */
    size_t arity;            /* число аргументов или ARITY_VARIABLE */
    CalcFunc func;           /* указатель на функцию, выполняющую команду */
    int enabled;             /* 1 - показывается в меню, 0 - скрыта */
} Command;

/* Реестр команд: динамический массив */
typedef struct {
    Command *items;
    size_t count;
    size_t capacity;
} Registry;

void registry_init(Registry *reg);
void registry_free(Registry *reg);

/* Регистрирует команду (строки копируются). 0 - ok, -1 - ошибка:
   пустое имя/описание, func == NULL, arity > MAX_ARGS, повторное имя, нет памяти. */
int registry_add(Registry *reg, const char *name, const char *description,
                 size_t arity, CalcFunc func);

/* Включает или выключает команду. 0 - ok, -1 - неверный индекс. */
int registry_set_enabled(Registry *reg, size_t index, int enabled);

const Command *registry_get(const Registry *reg, size_t index);   /* NULL при неверном индексе */
size_t registry_enabled_count(const Registry *reg);
const Command *registry_nth_enabled(const Registry *reg, size_t n); /* n с нуля; NULL если нет */

/* Вызывает функцию команды через указатель. Проверяет число аргументов. */
CalcStatus registry_execute(const Command *cmd, const double *args, size_t argc, double *result);

#endif /* REGISTRY_H */
