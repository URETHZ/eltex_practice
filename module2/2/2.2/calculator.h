#ifndef CALCULATOR_H
#define CALCULATOR_H

/* Код результата операции */
typedef enum {
    CALC_OK = 0,
    CALC_ERR_DIV_ZERO,   /* деление на ноль */
    CALC_ERR_DOMAIN,     /* аргумент вне области определения */
    CALC_ERR_OVERFLOW    /* результат слишком велик */
} CalcStatus;

/* Операции с вещественными числами. Результат записывается в *result,
   только если вернулся CALC_OK. */
CalcStatus calc_add(double a, double b, double *result);
CalcStatus calc_sub(double a, double b, double *result);
CalcStatus calc_mul(double a, double b, double *result);
CalcStatus calc_div(double a, double b, double *result);
CalcStatus calc_pow(double base, double exponent, double *result);
CalcStatus calc_sqrt(double a, double *result);

/* Операции с целыми числами */
CalcStatus calc_factorial(long n, double *result);   /* 0 <= n <= 170 */
CalcStatus calc_mod(long a, long b, long *result);   /* остаток от деления */

/* Текст сообщения для кода результата */
const char *calc_status_message(CalcStatus status);

#endif /* CALCULATOR_H */
