/* Библиотека калькулятора: одна функция "div".
   Собирается отдельно: plugins/div.dll (Windows) или plugins/div.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("div", "Division        (a / b)", 2)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    if (args[1] == 0.0) return CALC_ERR_DIV_ZERO;
    return calc_store(args[0] / args[1], result);
}
