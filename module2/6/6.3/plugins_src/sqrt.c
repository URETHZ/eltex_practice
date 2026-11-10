/* Библиотека калькулятора: одна функция "sqrt".
   Собирается отдельно: plugins/sqrt.dll (Windows) или plugins/sqrt.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("sqrt", "Square root", 1)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 1) return CALC_ERR_ARGC;
    if (args[0] < 0.0) return CALC_ERR_DOMAIN;
    return calc_store(sqrt(args[0]), result);
}
