/* Библиотека калькулятора: одна функция "cube".
   Собирается отдельно: plugins/cube.dll (Windows) или plugins/cube.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("cube", "Cube            (a ^ 3)", 1)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 1) return CALC_ERR_ARGC;
    return calc_store(args[0] * args[0] * args[0], result);
}
