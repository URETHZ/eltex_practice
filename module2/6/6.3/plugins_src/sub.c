/* Библиотека калькулятора: одна функция "sub".
   Собирается отдельно: plugins/sub.dll (Windows) или plugins/sub.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("sub", "Subtraction     (a - b)", 2)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    return calc_store(args[0] - args[1], result);
}
