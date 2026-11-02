#include <math.h>
#include <limits.h>
#include "calculator.h"

/* Записывает значение в *result, если оно конечное; иначе сообщает о переполнении. */
static CalcStatus store(double value, double *result)
{
    if (!isfinite(value)) return CALC_ERR_OVERFLOW;
    *result = value;
    return CALC_OK;
}

CalcStatus calc_add(double a, double b, double *result)
{
    return store(a + b, result);
}

CalcStatus calc_sub(double a, double b, double *result)
{
    return store(a - b, result);
}

CalcStatus calc_mul(double a, double b, double *result)
{
    return store(a * b, result);
}

CalcStatus calc_div(double a, double b, double *result)
{
    if (b == 0.0) return CALC_ERR_DIV_ZERO;
    return store(a / b, result);
}

CalcStatus calc_pow(double base, double exponent, double *result)
{
    if (base == 0.0 && exponent < 0.0) return CALC_ERR_DIV_ZERO;   /* 0^(-n) */
    /* отрицательное основание допустимо только при целой степени */
    if (base < 0.0 && exponent != floor(exponent)) return CALC_ERR_DOMAIN;
    return store(pow(base, exponent), result);
}

CalcStatus calc_sqrt(double a, double *result)
{
    if (a < 0.0) return CALC_ERR_DOMAIN;
    return store(sqrt(a), result);
}

CalcStatus calc_factorial(long n, double *result)
{
    if (n < 0) return CALC_ERR_DOMAIN;
    if (n > 170) return CALC_ERR_OVERFLOW;   /* 171! уже не помещается в double */

    double r = 1.0;
    for (long i = 2; i <= n; i++)
        r *= (double)i;
    *result = r;
    return CALC_OK;
}

CalcStatus calc_mod(long a, long b, long *result)
{
    if (b == 0) return CALC_ERR_DIV_ZERO;
    if (a == LONG_MIN && b == -1) {          /* LONG_MIN % -1 - неопределённое поведение */
        *result = 0;
        return CALC_OK;
    }
    *result = a % b;
    return CALC_OK;
}

const char *calc_status_message(CalcStatus status)
{
    switch (status) {
    case CALC_OK:           return "OK";
    case CALC_ERR_DIV_ZERO: return "Division by zero.";
    case CALC_ERR_DOMAIN:   return "Argument is out of the function's domain.";
    case CALC_ERR_OVERFLOW: return "Result is too large.";
    }
    return "Unknown error.";
}
