/* Библиотека калькулятора: одна функция "mod".
   Собирается отдельно: plugins/mod.dll (Windows) или plugins/mod.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("mod", "Remainder       (a % b, integers)", 2)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;

    long a, b;
    if (!calc_to_long(args[0], &a) || !calc_to_long(args[1], &b)) return CALC_ERR_DOMAIN;
    if (b == 0) return CALC_ERR_DIV_ZERO;
    *result = (double)(a % b);
    return CALC_OK;
}
