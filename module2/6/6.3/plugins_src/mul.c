/* Библиотека калькулятора: одна функция "mul".
   Собирается отдельно: plugins/mul.dll (Windows) или plugins/mul.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("mul", "Multiplication  (a * b)", 2)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    return calc_store(args[0] * args[1], result);
}
