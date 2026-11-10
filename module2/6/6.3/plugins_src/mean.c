/* Библиотека калькулятора: одна функция "mean".
   Собирается отдельно: plugins/mean.dll (Windows) или plugins/mean.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("mean", "Arithmetic mean of several numbers", CALC_ARITY_VARIABLE)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc < 1) return CALC_ERR_ARGC;
    double s = 0.0;
    for (size_t i = 0; i < argc; i++)
        s += args[i];
    return calc_store(s / (double)argc, result);
}
