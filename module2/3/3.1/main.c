#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "perms.h"

#ifndef HAVE_PWD_GRP
#  if defined(__unix__) || defined(__APPLE__)
#    define HAVE_PWD_GRP 1
#  else
#    define HAVE_PWD_GRP 0
#  endif
#endif

#if HAVE_PWD_GRP
#  include <pwd.h>
#  include <grp.h>
#endif

#define BUF_SIZE 4096

/* Текущая маска прав, с которой работает пункт "Изменить" */
typedef struct {
    unsigned mode;
    int defined;
} State; 

/* Читает строку, убирает пробелы по краям. 0 - конец ввода (EOF). */
static int read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin)) return 0;

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        len--;
    } else {                       /* строка не поместилась - сбрасываем остаток */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { }
    }
    buf[len] = '\0';

    size_t start = 0;
    while (buf[start] && isspace((unsigned char)buf[start])) start++;
    while (len > start && isspace((unsigned char)buf[len - 1])) len--;
    memmove(buf, buf + start, len - start);
    buf[len - start] = '\0';
    return 1;
}

/* Запрашивает номер пункта меню из [min, max]. 0 - EOF. */
static int read_choice(const char *prompt, long min, long max, long *out)
{
    char buf[64];
    for (;;) {
        if (!read_line(prompt, buf, sizeof buf)) return 0;

        char *end;
        errno = 0;
        long v = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && errno != ERANGE && v >= min && v <= max) {
            *out = v;
            return 1;
        }
        printf("Enter an integer from %ld to %ld.\n", min, max);
    }
}

/* Запись прав и команды chmod не могут содержать пробелов. */
static int has_whitespace(const char *s)
{
    return strpbrk(s, " \t") != NULL;
}

/* Запрашивает непустую строку без пробелов. 0 - EOF, -1 - отмена (пустая строка). */
static int read_token(const char *prompt, char *buf, size_t size)
{
    for (;;) {
        if (!read_line(prompt, buf, size)) return 0;
        if (buf[0] == '\0') {
            printf("Empty input, returning to the menu.\n");
            return -1;
        }
        if (has_whitespace(buf)) {
            printf("Spaces are not allowed here, try again.\n");
            continue;
        }
        return 1;
    }
}


static void print_mode(unsigned mode)
{
    char sym[PERM_SYM_SIZE], oct[PERM_OCT_SIZE], bin[PERM_BIN_SIZE];
    perm_to_symbolic(mode, sym);
    perm_to_octal(mode, oct);
    perm_to_binary(mode, bin);

    printf("  Letters: %s\n", sym);
    printf("  Digits:  %s\n", oct);
    printf("  Bits:    %s\n", bin);
    printf("           SGT rwx rwx rwx   (S=setuid, G=setgid, T=sticky)\n");
    printf("           spc usr grp oth\n");
}

static void print_owner_and_group(const FileInfo *fi, char *owner, size_t osize,
                                  char *group, size_t gsize)
{
#if HAVE_PWD_GRP
    struct passwd *pw = getpwuid((uid_t)fi->uid);
    struct group *gr = getgrgid((gid_t)fi->gid);

    if (pw) snprintf(owner, osize, "%s", pw->pw_name);
    else    snprintf(owner, osize, "%lu", fi->uid);
    if (gr) snprintf(group, gsize, "%s", gr->gr_name);
    else    snprintf(group, gsize, "%lu", fi->gid);
#else
    snprintf(owner, osize, "%lu", fi->uid);
    snprintf(group, gsize, "%lu", fi->gid);
#endif
}

/* Текущий umask процесса (нужен для команд вида "+x" без u/g/o/a) */
static unsigned current_umask(void)
{
#if HAVE_PWD_GRP
    mode_t m = umask(0);
    umask(m);
    return (unsigned)m;
#else
    return 022; // типичное значение, если umask недоступен 
#endif
}

static int action_enter(State *st)
{
    char buf[64];
    printf("\nExamples: 754, 4755, rwxr-xr--, rwsr-xr-x\n");
    int r = read_token("Permissions: ", buf, sizeof buf);
    if (r <= 0) return r == 0 ? 0 : 1;

    unsigned mode;
    PermStatus ps = perm_parse(buf, &mode);
    if (ps != PERM_OK) {
        printf("Error: %s\n", perm_status_message(ps));
        return 1;
    }
    st->mode = mode;
    st->defined = 1;
    print_mode(mode);
    return 1;
}

static int action_file(State *st)
{
    char path[BUF_SIZE];
    if (!read_line("\nFile name: ", path, sizeof path)) return 0;
    if (path[0] == '\0') {
        printf("Empty file name, returning to the menu.\n");
        return 1;
    }

    FileInfo fi;
    if (perm_stat(path, &fi) != 0) {
        printf("Error: cannot stat '%s': %s\n", path, strerror(errno));
        return 1;
    }

    char sym[PERM_SYM_SIZE], owner[64], group[64];
    perm_to_symbolic(fi.mode, sym);
    print_owner_and_group(&fi, owner, sizeof owner, group, sizeof group);

    /* Строка в формате ls -l (без даты) - для сравнения с настоящим ls -l */
    printf("  ls -l style: %c%s %lu %s %s %lld %s\n",
           fi.type, sym, fi.nlink, owner, group, fi.size, path);
    print_mode(fi.mode);

    st->mode = fi.mode;
    st->defined = 1;
    return 1;
}

static int action_modify(State *st)
{
    if (!st->defined) {
        printf("\nNo mask yet. Use item 1 or 2 first.\n");
        return 1;
    }

    printf("\nCurrent mask:\n");
    print_mode(st->mode);
    printf("Syntax: [ugoa][+-=][rwxst or u/g/o], comma-separated, or digits.\n"
           "Examples: u+x   g-w,o=r   a+rw   g=u   u+s   755\n");

    char buf[128];
    int r = read_token("Command: ", buf, sizeof buf);
    if (r <= 0) return r == 0 ? 0 : 1;

    unsigned mode = st->mode;
    PermStatus ps = perm_apply_chmod(&mode, buf, current_umask());
    if (ps != PERM_OK) {
        printf("Error: %s\n", perm_status_message(ps));
        return 1;
    }

    st->mode = mode;
    printf("New mask (not applied to any file):\n");
    print_mode(mode);
    return 1;
}

int main(void)
{
    State st = { 0, 0 };

    for (;;) {
        printf("\n=== Permission mask calculator ===\n"
               "1) Enter permissions (letters or digits)\n"
               "2) Show permissions of a file (stat)\n"
               "3) Modify current mask (chmod-like)\n"
               "0) Exit\n");

        long choice;
        if (!read_choice("Action: ", 0, 3, &choice) || choice == 0) break;

        int ok = 1;                      /* 0 - ввод оборвался (EOF) */
        switch (choice) {
        case 1: ok = action_enter(&st); break;
        case 2: ok = action_file(&st); break;
        case 3: ok = action_modify(&st); break;
        }
        if (!ok) break;
    }

    printf("Bye.\n");
    return 0;
}
