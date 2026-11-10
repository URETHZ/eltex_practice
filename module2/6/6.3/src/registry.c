#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "registry.h"

void registry_init(Registry *reg)
{
    reg->items = NULL;
    reg->count = 0;
}

void registry_free(Registry *reg)
{
    for (size_t i = 0; i < reg->count; i++)
        free(reg->items[i].path);
    free(reg->items);
    registry_init(reg);
}

static const Command *find_by_name(const Registry *reg, const char *name)
{
    for (size_t i = 0; i < reg->count; i++)
        if (strcmp(reg->items[i].info.name, name) == 0) return &reg->items[i];
    return NULL;
}

static int compare_commands(const void *a, const void *b)
{
    return strcmp(((const Command *)a)->info.name, ((const Command *)b)->info.name);
}

/* Добавляет команду в конец реестра (путь копируется). 0 - ok, -1 - нет памяти. */
static int append_command(Registry *reg, const PluginInfo *info, const char *path)
{
    size_t len = strlen(path) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) return -1;
    memcpy(copy, path, len);

    Command *tmp = (Command *)realloc(reg->items, (reg->count + 1) * sizeof(Command));
    if (!tmp) {
        free(copy);
        return -1;
    }
    reg->items = tmp;
    reg->items[reg->count].info = *info;
    reg->items[reg->count].path = copy;
    reg->count++;
    return 0;
}

/* Имя файла без каталога - для сообщений */
static const char *base_name(const char *path)
{
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    return base;
}

static void report_name(char list[][CALC_NAME_MAX], size_t *count, const char *name)
{
    if (*count < REPORT_MAX) snprintf(list[*count], CALC_NAME_MAX, "%s", name);
    (*count)++;
}

static void report_skipped(RefreshReport *rep, const char *path, const char *reason, const char *detail)
{
    if (!rep) return;
    if (rep->skipped_count < REPORT_MAX) {
        if (detail && detail[0])
            snprintf(rep->skipped[rep->skipped_count], REPORT_TEXT_MAX, "%s: %s (%s)",
                     base_name(path), reason, detail);
        else
            snprintf(rep->skipped[rep->skipped_count], REPORT_TEXT_MAX, "%s: %s",
                     base_name(path), reason);
    }
    rep->skipped_count++;
}

int registry_refresh(Registry *reg, const char *dir, RefreshReport *report)
{
    RefreshReport local;
    RefreshReport *rep = report ? report : &local;
    memset(rep, 0, sizeof *rep);

    char **files = NULL;
    size_t nfiles = 0;
    int rc = plugin_list_files(dir, &files, &nfiles);
    if (rc != 0) return rc;                      /* реестр не тронут */
    rep->scanned = nfiles;

    /* Строим новый список целиком и только потом подменяем старый */
    Registry fresh;
    registry_init(&fresh);

    for (size_t i = 0; i < nfiles; i++) {
        PluginInfo info;
        char detail[128];
        LoadStatus st = plugin_probe(files[i], &info, detail, sizeof detail);   /* загрузка и сразу выгрузка */

        if (st != LOAD_OK) {
            report_skipped(rep, files[i], load_status_message(st), detail);
            continue;
        }
        if (find_by_name(&fresh, info.name)) {
            char why[96];
            snprintf(why, sizeof why, "command '%s' is already provided by another library", info.name);
            report_skipped(rep, files[i], why, NULL);
            continue;
        }
        if (append_command(&fresh, &info, files[i]) != 0) {
            registry_free(&fresh);
            plugin_free_list(files, nfiles);
            return -2;
        }
    }
    plugin_free_list(files, nfiles);

    if (fresh.count > 1) qsort(fresh.items, fresh.count, sizeof(Command), compare_commands);

    /* Что изменилось по сравнению с прежним меню */
    for (size_t i = 0; i < fresh.count; i++) {
        const Command *c = &fresh.items[i];
        const Command *old = find_by_name(reg, c->info.name);
        if (!old) {
            report_name(rep->added, &rep->added_count, c->info.name);
        } else if (strcmp(old->path, c->path) != 0 || old->info.arity != c->info.arity ||
                   strcmp(old->info.description, c->info.description) != 0) {
            report_name(rep->updated, &rep->updated_count, c->info.name);
        }
    }
    for (size_t i = 0; i < reg->count; i++)
        if (!find_by_name(&fresh, reg->items[i].info.name))
            report_name(rep->removed, &rep->removed_count, reg->items[i].info.name);

    registry_free(reg);
    *reg = fresh;
    return 0;
}

LoadStatus registry_execute(const Command *cmd, const double *args, size_t argc,
                            double *result, CalcStatus *status,
                            char *detail, size_t detail_size)
{
    detail[0] = '\0';
    *status = CALC_OK;

    /* Число аргументов проверяем до загрузки библиотеки */
    if (argc == 0 || argc > CALC_MAX_ARGS ||
        (cmd->info.arity != CALC_ARITY_VARIABLE && argc != cmd->info.arity)) {
        *status = CALC_ERR_ARGC;
        return LOAD_OK;
    }
    return plugin_call(cmd->path, &cmd->info, args, argc, result, status, detail, detail_size);
}
