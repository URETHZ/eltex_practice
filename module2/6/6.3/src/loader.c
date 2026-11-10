#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "loader.h"

/* =====================  ПЛАТФОРМЕННЫЙ СЛОЙ  =====================
   Всё, что отличается в Windows и в Linux/macOS, собрано здесь. Выше по
   файлу и в остальных модулях программы платформенного кода нет. */

#ifdef _WIN32
#  include <windows.h>
#  define PLUGIN_EXT ".dll"
#  define PATH_SEP '\\'
#else
#  include <dirent.h>
#  include <dlfcn.h>
#  ifdef __APPLE__
#    define PLUGIN_EXT ".dylib"
#  else
#    define PLUGIN_EXT ".so"
#  endif
#  define PATH_SEP '/'
#endif

/* Универсальный тип указателя на функцию: из него приводим к нужному типу */
typedef void (*GenericFn)(void);

#ifdef _WIN32

typedef HMODULE LibHandle;

static LibHandle lib_open(const char *path, char *detail, size_t size)
{
    HMODULE h = LoadLibraryA(path);
    if (!h) snprintf(detail, size, "system error code %lu", (unsigned long)GetLastError());
    return h;
}

static GenericFn lib_symbol(LibHandle h, const char *name)
{
    return (GenericFn)GetProcAddress(h, name);
}

/* После FreeLibrary Windows снимает блокировку с файла: его можно удалить */
static void lib_close(LibHandle h)
{
    FreeLibrary(h);
}

#else

typedef void *LibHandle;

static LibHandle lib_open(const char *path, char *detail, size_t size)
{
    dlerror();                                   /* сбросить прежнюю ошибку */
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char *e = dlerror();
        snprintf(detail, size, "%s", e ? e : "unknown error");
    }
    return h;
}

static GenericFn lib_symbol(LibHandle h, const char *name)
{
    void *p = dlsym(h, name);
    GenericFn f;
    memcpy(&f, &p, sizeof f);                    /* приведение void * к указателю на функцию без предупреждений */
    return f;
}

static void lib_close(LibHandle h)
{
    dlclose(h);
}

#endif

/* =====================  ОПИСАНИЕ КОМАНДЫ  ===================== */

static int valid_name(const char *s)
{
    if (!s || s[0] == '\0' || strlen(s) >= CALC_NAME_MAX) return 0;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-') return 0;
    return 1;
}

/* Читает и проверяет описание команды из загруженной библиотеки */
static LoadStatus read_info(LibHandle h, PluginInfo *out, char *detail, size_t size)
{
    GenericFn info_fn = lib_symbol(h, CALC_PLUGIN_INFO_SYMBOL);
    GenericFn run_fn = lib_symbol(h, CALC_PLUGIN_RUN_SYMBOL);
    if (!info_fn || !run_fn) return LOAD_ERR_SYMBOLS;

    const CalcPluginInfo *src = ((CalcPluginInfoFn)info_fn)();
    if (!src) {
        snprintf(detail, size, "calc_plugin_info returned NULL");
        return LOAD_ERR_INFO;
    }
    if (src->api_version != CALC_PLUGIN_API_VERSION) {
        snprintf(detail, size, "library uses API version %d, program expects %d",
                 src->api_version, CALC_PLUGIN_API_VERSION);
        return LOAD_ERR_VERSION;
    }
    if (!valid_name(src->name)) {
        snprintf(detail, size, "command name must be 1-%d characters: letters, digits, _ or -",
                 CALC_NAME_MAX - 1);
        return LOAD_ERR_INFO;
    }
    if (src->arity > CALC_MAX_ARGS) {
        snprintf(detail, size, "arity %u exceeds the maximum %d", src->arity, CALC_MAX_ARGS);
        return LOAD_ERR_INFO;
    }

    /* Строки копируем: после выгрузки библиотеки её память станет недоступной */
    snprintf(out->name, sizeof out->name, "%s", src->name);
    snprintf(out->description, sizeof out->description, "%s",
             (src->description && src->description[0]) ? src->description : src->name);
    out->arity = src->arity;
    return LOAD_OK;
}

/* =====================  ЗАГРУЗКА НА ВРЕМЯ ОПЕРАЦИИ  ===================== */

LoadStatus plugin_probe(const char *path, PluginInfo *info, char *detail, size_t detail_size)
{
    detail[0] = '\0';
    LibHandle h = lib_open(path, detail, detail_size);
    if (!h) return LOAD_ERR_OPEN;

    LoadStatus st = read_info(h, info, detail, detail_size);
    lib_close(h);                                /* файл сразу освобождается */
    return st;
}

LoadStatus plugin_call(const char *path, const PluginInfo *expected,
                       const double *args, size_t argc, double *result,
                       CalcStatus *status, char *detail, size_t detail_size)
{
    detail[0] = '\0';
    LibHandle h = lib_open(path, detail, detail_size);
    if (!h) return LOAD_ERR_OPEN;

    PluginInfo now;
    LoadStatus st = read_info(h, &now, detail, detail_size);
    if (st == LOAD_OK) {
        if (strcmp(now.name, expected->name) != 0 || now.arity != expected->arity) {
            snprintf(detail, detail_size, "the file now provides command '%s' (arguments: %u)",
                     now.name, now.arity);
            st = LOAD_ERR_CHANGED;
        } else {
            CalcPluginRunFn run = (CalcPluginRunFn)lib_symbol(h, CALC_PLUGIN_RUN_SYMBOL);
            *status = run(args, argc, result);
        }
    }
    lib_close(h);                                /* выгружаем сразу после вызова */
    return st;
}

const char *load_status_message(LoadStatus status)
{
    switch (status) {
    case LOAD_OK:          return "OK";
    case LOAD_ERR_OPEN:    return "the library cannot be loaded (removed, damaged or built for another system)";
    case LOAD_ERR_SYMBOLS: return "not a calculator library (calc_plugin_info / calc_plugin_run not found)";
    case LOAD_ERR_VERSION: return "incompatible library version";
    case LOAD_ERR_INFO:    return "invalid command description";
    case LOAD_ERR_CHANGED: return "the library was replaced after the menu was built";
    }
    return "unknown error";
}

/* =====================  СПИСОК ФАЙЛОВ КАТАЛОГА  ===================== */

/* Расширение сравниваем без учёта регистра: в Windows бывает "ADD.DLL" */
static int has_plugin_ext(const char *name)
{
    size_t n = strlen(name), e = strlen(PLUGIN_EXT);
    if (n <= e) return 0;
    const char *tail = name + n - e;
    for (size_t i = 0; i < e; i++)
        if (tolower((unsigned char)tail[i]) != PLUGIN_EXT[i]) return 0;
    return 1;
}

/* dir + разделитель + name (завершающие разделители dir отбрасываются) */
static char *join_path(const char *dir, const char *name)
{
    size_t dl = strlen(dir);
    while (dl > 1 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\')) dl--;

    char *p = (char *)malloc(dl + 1 + strlen(name) + 1);
    if (!p) return NULL;
    memcpy(p, dir, dl);
    p[dl] = PATH_SEP;
    strcpy(p + dl + 1, name);
#ifdef _WIN32
    for (char *c = p; *c; c++)
        if (*c == '/') *c = '\\';
#endif
    return p;
}

static int push_path(char ***paths, size_t *count, size_t *capacity, char *path)
{
    if (*count == *capacity) {
        size_t new_cap = *capacity ? *capacity * 2 : 16;
        char **tmp = (char **)realloc(*paths, new_cap * sizeof(char *));
        if (!tmp) return -1;
        *paths = tmp;
        *capacity = new_cap;
    }
    (*paths)[(*count)++] = path;
    return 0;
}

static int compare_paths(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

void plugin_free_list(char **paths, size_t count)
{
    for (size_t i = 0; i < count; i++) free(paths[i]);
    free(paths);
}

int plugin_list_files(const char *dir, char ***paths_out, size_t *count_out)
{
    char **paths = NULL;
    size_t count = 0, capacity = 0;
    int failed = 0;

#ifdef _WIN32
    size_t dl = strlen(dir);
    while (dl > 1 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\')) dl--;
    char *pattern = (char *)malloc(dl + 3);
    if (!pattern) return -2;
    memcpy(pattern, dir, dl);
    strcpy(pattern + dl, "\\*");

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    free(pattern);
    if (h == INVALID_HANDLE_VALUE) return -1;

    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !has_plugin_ext(fd.cFileName))
            continue;
        char *p = join_path(dir, fd.cFileName);
        if (!p || push_path(&paths, &count, &capacity, p) != 0) { free(p); failed = 1; break; }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) return -1;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!has_plugin_ext(e->d_name)) continue;
        char *p = join_path(dir, e->d_name);
        if (!p || push_path(&paths, &count, &capacity, p) != 0) { free(p); failed = 1; break; }
    }
    closedir(d);
#endif

    if (failed) {
        plugin_free_list(paths, count);
        return -2;
    }
    if (count > 1) qsort(paths, count, sizeof(char *), compare_paths);

    *paths_out = paths;
    *count_out = count;
    return 0;
}
