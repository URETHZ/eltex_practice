#include <math.h>
#include "operations.h"

/* Записывает значение в *result, если оно конечное; иначе сообщает о переполнении. */
static CalcStatus store(double value, double *result)
{
    if (!isfinite(value)) return CALC_ERR_OVERFLOW;
    *result = value;
    return CALC_OK;
}

/* Преобразует double в long, если число целое и безопасно помещается. */
static int to_long(double x, long *out)
{
    if (!isfinite(x) || x != floor(x) || fabs(x) > 9e15) return 0;
    *out = (long)x;
    return 1;
}

/* ---------- фиксированное число аргументов ---------- */

CalcStatus op_add(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    return store(args[0] + args[1], result);
}

CalcStatus op_sub(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    return store(args[0] - args[1], result);
}

CalcStatus op_mul(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    return store(args[0] * args[1], result);
}

CalcStatus op_div(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    if (args[1] == 0.0) return CALC_ERR_DIV_ZERO;
    return store(args[0] / args[1], result);
}

CalcStatus op_pow(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    double base = args[0], exponent = args[1];
    if (base == 0.0 && exponent < 0.0) return CALC_ERR_DIV_ZERO;   /* 0^(-n) */
    /* отрицательное основание допустимо только при целой степени */
    if (base < 0.0 && exponent != floor(exponent)) return CALC_ERR_DOMAIN;
    return store(pow(base, exponent), result);
}

CalcStatus op_sqrt(const double *args, size_t argc, double *result)
{
    if (argc != 1) return CALC_ERR_ARGC;
    if (args[0] < 0.0) return CALC_ERR_DOMAIN;
    return store(sqrt(args[0]), result);
}

CalcStatus op_factorial(const double *args, size_t argc, double *result)
{
    if (argc != 1) return CALC_ERR_ARGC;

    long n;
    if (!to_long(args[0], &n) || n < 0) return CALC_ERR_DOMAIN;
    if (n > 170) return CALC_ERR_OVERFLOW;   /* 171! не помещается в double */

    double r = 1.0;
    for (long i = 2; i <= n; i++)
        r *= (double)i;
    *result = r;
    return CALC_OK;
}

CalcStatus op_mod(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;

    long a, b;
    if (!to_long(args[0], &a) || !to_long(args[1], &b)) return CALC_ERR_DOMAIN;
    if (b == 0) return CALC_ERR_DIV_ZERO;
    *result = (double)(a % b);
    return CALC_OK;
}

/* ---------- произвольное число аргументов ---------- */

CalcStatus op_sum(const double *args, size_t argc, double *result)
{
    if (argc < 1) return CALC_ERR_ARGC;
    double s = 0.0;
    for (size_t i = 0; i < argc; i++)
        s += args[i];
    return store(s, result);
}

CalcStatus op_mean(const double *args, size_t argc, double *result)
{
    if (argc < 1) return CALC_ERR_ARGC;
    double s = 0.0;
    for (size_t i = 0; i < argc; i++)
        s += args[i];
    return store(s / (double)argc, result);
}

CalcStatus op_max(const double *args, size_t argc, double *result)
{
    if (argc < 1) return CALC_ERR_ARGC;
    double m = args[0];
    for (size_t i = 1; i < argc; i++)
        if (args[i] > m) m = args[i];
    *result = m;
    return CALC_OK;
}

const char *calc_status_message(CalcStatus status)
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
