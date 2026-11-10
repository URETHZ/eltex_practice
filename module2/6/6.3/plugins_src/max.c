/* Библиотека калькулятора: одна функция "max".
   Собирается отдельно: plugins/max.dll (Windows) или plugins/max.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("max", "Maximum of several numbers", CALC_ARITY_VARIABLE)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc < 1) return CALC_ERR_ARGC;
    double m = args[0];
    for (size_t i = 1; i < argc; i++)
        if (args[i] > m) m = args[i];
    *result = m;
    return CALC_OK;
}
