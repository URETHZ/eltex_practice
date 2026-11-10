/* Библиотека калькулятора: одна функция "fact".
   Собирается отдельно: plugins/fact.dll (Windows) или plugins/fact.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("fact", "Factorial       (n!, integer)", 1)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 1) return CALC_ERR_ARGC;

    long n;
    if (!calc_to_long(args[0], &n) || n < 0) return CALC_ERR_DOMAIN;
    if (n > 170) return CALC_ERR_OVERFLOW;   /* 171! не помещается в double */

    double r = 1.0;
    for (long i = 2; i <= n; i++)
        r *= (double)i;
    *result = r;
    return CALC_OK;
}
