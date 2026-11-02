#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stddef.h>

/* Код результата операции */
typedef enum {
    CALC_OK = 0,
    CALC_ERR_DIV_ZERO,   /* деление на ноль */
    CALC_ERR_DOMAIN,     /* аргумент вне области определения */
    CALC_ERR_OVERFLOW,   /* результат слишком велик */
    CALC_ERR_ARGC        /* неверное число аргументов */
} CalcStatus;

/* Единая сигнатура любой операции калькулятора.
   Именно она позволяет хранить операции в реестре как указатели на функции.
   Результат записывается в *result только при CALC_OK. */
typedef CalcStatus (*CalcFunc)(const double *args, size_t argc, double *result);

/* Операции с фиксированным числом аргументов */
CalcStatus op_add(const double *args, size_t argc, double *result);
CalcStatus op_sub(const double *args, size_t argc, double *result);
CalcStatus op_mul(const double *args, size_t argc, double *result);
CalcStatus op_div(const double *args, size_t argc, double *result);
CalcStatus op_pow(const double *args, size_t argc, double *result);
CalcStatus op_sqrt(const double *args, size_t argc, double *result);
CalcStatus op_factorial(const double *args, size_t argc, double *result);  /* целое 0..170 */
CalcStatus op_mod(const double *args, size_t argc, double *result);        /* целые числа */

/* Операции с произвольным числом аргументов (argc >= 1) */
CalcStatus op_sum(const double *args, size_t argc, double *result);
CalcStatus op_mean(const double *args, size_t argc, double *result);
CalcStatus op_max(const double *args, size_t argc, double *result);

/* Текст сообщения для кода результата */
const char *calc_status_message(CalcStatus status);

#endif /* OPERATIONS_H */
