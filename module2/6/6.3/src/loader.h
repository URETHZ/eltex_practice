#ifndef LOADER_H
#define LOADER_H

#include <stddef.h>
#include "plugin_api.h"

/* Загрузчик библиотек с функциями.

   Главное правило: библиотека загружается только на время одной операции
   (чтение описания или вызов функции) и сразу выгружается. Между операциями
   ни один файл из каталога не открыт и не занят, поэтому его можно удалить,
   заменить или добавить новый, пока программа работает. */

typedef enum {
    LOAD_OK = 0,
    LOAD_ERR_OPEN,      /* файл не удалось загрузить (удалён, повреждён, не та платформа) */
    LOAD_ERR_SYMBOLS,   /* в библиотеке нет calc_plugin_info / calc_plugin_run */
    LOAD_ERR_VERSION,   /* библиотека собрана под другую версию интерфейса */
    LOAD_ERR_INFO,      /* описание команды некорректно */
    LOAD_ERR_CHANGED    /* библиотеку заменили: команда теперь другая */
} LoadStatus;

/* Описание команды, скопированное из библиотеки (строки живут в самой структуре,
   потому что после выгрузки библиотеки её память недоступна) */
typedef struct {
    char name[CALC_NAME_MAX];
    char description[CALC_DESC_MAX];
    unsigned arity;
} PluginInfo;

/* Загружает библиотеку, читает описание в *info и сразу выгружает её.
   При ошибке в detail (может быть пустой) - уточнение от системы. */
LoadStatus plugin_probe(const char *path, PluginInfo *info, char *detail, size_t detail_size);

/* Загружает библиотеку, вызывает calc_plugin_run и выгружает её.
   expected - описание, известное меню: если библиотеку успели заменить
   на другую команду, вызов не выполняется (LOAD_ERR_CHANGED).
   Результат самой функции возвращается в *status (при LOAD_OK). */
LoadStatus plugin_call(const char *path, const PluginInfo *expected,
                       const double *args, size_t argc, double *result,
                       CalcStatus *status, char *detail, size_t detail_size);

const char *load_status_message(LoadStatus status);

/* Список файлов библиотек (с расширением .dll/.so/.dylib) в каталоге dir.
   Пути полные (dir + разделитель + имя), отсортированы по имени.
   0 - успех, -1 - каталог не читается, -2 - нет памяти. */
int plugin_list_files(const char *dir, char ***paths, size_t *count);
void plugin_free_list(char **paths, size_t count);

#endif /* LOADER_H */
