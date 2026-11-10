/* Библиотека калькулятора: одна функция "pow".
   Собирается отдельно: plugins/pow.dll (Windows) или plugins/pow.so (Linux). */

#include "plugin_api.h"

CALC_DEFINE_PLUGIN_INFO("pow", "Power           (a ^ b)", 2)

CALC_PLUGIN_EXPORT CalcStatus calc_plugin_run(const double *args, size_t argc, double *result)
{
    if (argc != 2) return CALC_ERR_ARGC;
    double base = args[0], exponent = args[1];
    if (base == 0.0 && exponent < 0.0) return CALC_ERR_DIV_ZERO;   /* 0^(-n) */
    /* отрицательное основание допустимо только при целой степени */
    if (base < 0.0 && exponent != floor(exponent)) return CALC_ERR_DOMAIN;
    return calc_store(pow(base, exponent), result);
}
