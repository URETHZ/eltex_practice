#ifndef REGISTRY_H
#define REGISTRY_H

#include <stddef.h>
#include "plugin_api.h"
#include "loader.h"

/* Одна команда меню: её описание и путь к библиотеке.
   Сама библиотека при этом НЕ загружена - загрузка происходит только при вызове. */
typedef struct {
    PluginInfo info;
    char *path;
} Command;

typedef struct {
    Command *items;        /* отсортированы по имени команды */
    size_t count;
} Registry;

#define REPORT_MAX 32      /* сколько имён каждого вида запоминается для вывода */
#define REPORT_TEXT_MAX 200

/* Что изменилось при обновлении. В массивах - первые REPORT_MAX записей,
   в *_count - реальное количество. */
typedef struct {
    size_t scanned;                                   /* найдено файлов библиотек */
    size_t added_count, removed_count, updated_count, skipped_count;
    char added[REPORT_MAX][CALC_NAME_MAX];            /* появились в меню */
    char removed[REPORT_MAX][CALC_NAME_MAX];          /* пропали из меню */
    char updated[REPORT_MAX][CALC_NAME_MAX];          /* остались, но библиотека заменена */
    char skipped[REPORT_MAX][REPORT_TEXT_MAX];        /* "файл: причина" */
} RefreshReport;

void registry_init(Registry *reg);
void registry_free(Registry *reg);

/* Перечитывает каталог: каждая библиотека кратко загружается, из неё читается
   описание команды, и библиотека сразу выгружается. Список команд заменяется
   найденными (исчезнувшие файлы вычёркиваются, новые добавляются).
   0 - успех; -1 - каталог не читается; -2 - нет памяти.
   При ошибке реестр остаётся прежним. report может быть NULL. */
int registry_refresh(Registry *reg, const char *dir, RefreshReport *report);

/* Выполняет команду: проверяет число аргументов и вызывает функцию из библиотеки,
   загружая её на время вызова. Возвращает статус загрузки; результат вычисления -
   в *status (если статус загрузки LOAD_OK). */
LoadStatus registry_execute(const Command *cmd, const double *args, size_t argc,
                            double *result, CalcStatus *status,
                            char *detail, size_t detail_size);

#endif /* REGISTRY_H */
