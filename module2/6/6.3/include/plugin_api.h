#ifndef PLUGIN_API_H
#define PLUGIN_API_H

/* Контракт между калькулятором и библиотеками с функциями.
   Заголовок подключают и программа, и каждая библиотека.

   В одной библиотеке - одна функция калькулятора. Библиотека экспортирует
   ровно два имени:

       const CalcPluginInfo *calc_plugin_info(void);    - описание команды
       CalcStatus calc_plugin_run(const double *args, size_t argc, double *result);

   Для написания новой библиотеки достаточно подключить этот файл, описать
   команду макросом CALC_DEFINE_PLUGIN_INFO и реализовать calc_plugin_run. */

#include <math.h>
#include <stddef.h>

#define CALC_PLUGIN_API_VERSION 1

#define CALC_ARITY_VARIABLE 0    /* число аргументов вводит пользователь */
#define CALC_MAX_ARGS 16         /* максимум аргументов у одной команды */
#define CALC_NAME_MAX 32         /* длина имени команды с учётом '\0' */
#define CALC_DESC_MAX 96         /* длина описания с учётом '\0' */

/* Результат вычисления */
typedef enum {
    CALC_OK = 0,
    CALC_ERR_DIV_ZERO,   /* деление на ноль */
    CALC_ERR_DOMAIN,     /* аргумент вне области определения */
    CALC_ERR_OVERFLOW,   /* результат слишком велик */
    CALC_ERR_ARGC        /* неверное число аргументов */
} CalcStatus;

/* Описание команды, которое библиотека сообщает программе */
typedef struct {
    int api_version;           /* CALC_PLUGIN_API_VERSION */
    const char *name;          /* короткое имя: "add" */
    const char *description;   /* строка пункта меню: "Addition (a + b)" */
    unsigned arity;            /* число аргументов или CALC_ARITY_VARIABLE */
} CalcPluginInfo;

/* Экспорт функций из библиотеки: для Windows нужен __declspec(dllexport) */
#ifdef _WIN32
#  define CALC_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define CALC_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/* Типы экспортируемых функций и их имена в библиотеке */
typedef const CalcPluginInfo *(*CalcPluginInfoFn)(void);
typedef CalcStatus (*CalcPluginRunFn)(const double *args, size_t argc, double *result);

#define CALC_PLUGIN_INFO_SYMBOL "calc_plugin_info"
#define CALC_PLUGIN_RUN_SYMBOL  "calc_plugin_run"

/* Описывает команду библиотеки и экспортирует calc_plugin_info */
#define CALC_DEFINE_PLUGIN_INFO(NAME, DESC, ARITY)                              \
    static const CalcPluginInfo calc_plugin_info_data = {                       \
        CALC_PLUGIN_API_VERSION, NAME, DESC, ARITY                              \
    };                                                                          \
    CALC_PLUGIN_EXPORT const CalcPluginInfo *calc_plugin_info(void)             \
    {                                                                           \
        return &calc_plugin_info_data;                                          \
    }

/* Записывает значение в *result, если оно конечное; иначе сообщает о переполнении. */
static inline CalcStatus calc_store(double value, double *result)
{
    if (!isfinite(value)) return CALC_ERR_OVERFLOW;
    *result = value;
    return CALC_OK;
}

/* Преобразует double в long, если число целое и безопасно помещается. */
static inline int calc_to_long(double x, long *out)
{
    if (!isfinite(x) || x != floor(x) || fabs(x) > 9e15) return 0;
    *out = (long)x;
    return 1;
}

static inline const char *calc_status_message(CalcStatus status)
{
    switch (status) {
    case CALC_OK:           return "OK";
    case CALC_ERR_DIV_ZERO: return "Division by zero.";
    case CALC_ERR_DOMAIN:   return "Argument is out of the function's domain.";
    case CALC_ERR_OVERFLOW: return "Result is too large.";
    case CALC_ERR_ARGC:     return "Wrong number of arguments.";
    }
    return "Unknown error.";
}

#endif /* PLUGIN_API_H */
