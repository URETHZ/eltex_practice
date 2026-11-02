#include <stdlib.h>
#include <string.h>
#include "registry.h"

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

void registry_init(Registry *reg)
{
    reg->items = NULL;
    reg->count = 0;
    reg->capacity = 0;
}

void registry_free(Registry *reg)
{
    for (size_t i = 0; i < reg->count; i++) {
        free(reg->items[i].name);
        free(reg->items[i].description);
    }
    free(reg->items);
    registry_init(reg);
}

int registry_add(Registry *reg, const char *name, const char *description,
                 size_t arity, CalcFunc func)
{
    if (!name || !description || !func) return -1;
    if (name[0] == '\0' || description[0] == '\0') return -1;
    if (arity > MAX_ARGS) return -1;

    for (size_t i = 0; i < reg->count; i++)
        if (strcmp(reg->items[i].name, name) == 0) return -1;   /* дубликат */

    if (reg->count == reg->capacity) {
        size_t new_cap = reg->capacity ? reg->capacity * 2 : 8;
        Command *tmp = (Command *)realloc(reg->items, new_cap * sizeof(Command));
        if (!tmp) return -1;
        reg->items = tmp;
        reg->capacity = new_cap;
    }

    char *n = dup_str(name);
    char *d = dup_str(description);
    if (!n || !d) {
        free(n);
        free(d);
        return -1;
    }

    Command *cmd = &reg->items[reg->count++];
    cmd->name = n;
    cmd->description = d;
    cmd->arity = arity;
    cmd->func = func;
    cmd->enabled = 1;
    return 0;
}

int registry_set_enabled(Registry *reg, size_t index, int enabled)
{
    if (index >= reg->count) return -1;
    reg->items[index].enabled = enabled ? 1 : 0;
    return 0;
}

const Command *registry_get(const Registry *reg, size_t index)
{
    return index < reg->count ? &reg->items[index] : NULL;
}

size_t registry_enabled_count(const Registry *reg)
{
    size_t k = 0;
    for (size_t i = 0; i < reg->count; i++)
        if (reg->items[i].enabled) k++;
    return k;
}

const Command *registry_nth_enabled(const Registry *reg, size_t n)
{
    for (size_t i = 0; i < reg->count; i++) {
        if (!reg->items[i].enabled) continue;
        if (n == 0) return &reg->items[i];
        n--;
    }
    return NULL;
}

CalcStatus registry_execute(const Command *cmd, const double *args, size_t argc, double *result)
{
    if (!cmd || !cmd->func) return CALC_ERR_ARGC;
    if (argc == 0 || argc > MAX_ARGS) return CALC_ERR_ARGC;
    if (cmd->arity != ARITY_VARIABLE && argc != cmd->arity) return CALC_ERR_ARGC;

    return cmd->func(args, argc, result);   /* вызов через указатель на функцию */
}
