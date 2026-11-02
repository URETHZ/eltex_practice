#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "perms.h"

#define BIT_SETUID 04000u
#define BIT_SETGID 02000u
#define BIT_STICKY 01000u
#define MODE_MASK  07777u

/* Номера классов: 0 - владелец (u), 1 - группа (g), 2 - остальные (o) */
static unsigned class_shift(int cls)
{
    return (unsigned)(6 - 3 * cls);
}

/* Какие биты "принадлежат" классу (с учётом специальных, как в GNU chmod) */
static unsigned class_mask(int cls)
{
    static const unsigned masks[3] = {
        BIT_SETUID | 0700u,   /* u: setuid + rwx владельца */
        BIT_SETGID | 0070u,   /* g: setgid + rwx группы    */
        BIT_STICKY | 0007u    /* o: sticky + rwx остальных */
    };
    return masks[cls];
}

static int all_digits(const char *s)
{
    if (*s == '\0') return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

PermStatus perm_parse_octal(const char *text, unsigned *mode)
{
    if (!text || text[0] == '\0') return PERM_ERR_EMPTY;

    size_t len = strlen(text);
    unsigned value = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] < '0' || text[i] > '7') return PERM_ERR_OCTAL_DIGIT;
        value = value * 8 + (unsigned)(text[i] - '0');
    }
    if (len > 4) return PERM_ERR_LENGTH;

    *mode = value;
    return PERM_OK;
}

PermStatus perm_parse_symbolic(const char *text, unsigned *mode)
{
    if (!text || text[0] == '\0') return PERM_ERR_EMPTY;

    size_t len = strlen(text);
    if (len == 10) {                       /* допускаем первый символ типа файла, как в ls -l */
        if (!strchr("-dlcbps", text[0])) return PERM_ERR_SYMBOLIC;
        text++;
        len = 9;
    }
    if (len != 9) return PERM_ERR_SYMBOLIC;

    static const unsigned special[3] = { BIT_SETUID, BIT_SETGID, BIT_STICKY };
    unsigned m = 0;

    for (int cls = 0; cls < 3; cls++) {
        const char *t = text + cls * 3;
        unsigned shift = class_shift(cls);

        if (t[0] == 'r')      m |= 4u << shift;
        else if (t[0] != '-') return PERM_ERR_SYMBOLIC;

        if (t[1] == 'w')      m |= 2u << shift;
        else if (t[1] != '-') return PERM_ERR_SYMBOLIC;

        char x = t[2];
        if (x == 'x') {
            m |= 1u << shift;
        } else if ((x == 's' && cls < 2) || (x == 't' && cls == 2)) {
            m |= (1u << shift) | special[cls];       /* спецбит + x */
        } else if ((x == 'S' && cls < 2) || (x == 'T' && cls == 2)) {
            m |= special[cls];                        /* спецбит без x */
        } else if (x != '-') {
            return PERM_ERR_SYMBOLIC;
        }
    }
    *mode = m;
    return PERM_OK;
}

PermStatus perm_parse(const char *text, unsigned *mode)
{
    if (!text || text[0] == '\0') return PERM_ERR_EMPTY;
    return all_digits(text) ? perm_parse_octal(text, mode)
                            : perm_parse_symbolic(text, mode);
}

/* ---------- модификация (аналог chmod) ---------- */

PermStatus perm_apply_chmod(unsigned *mode, const char *expr, unsigned umask_value)
{
    if (!expr || expr[0] == '\0') return PERM_ERR_EMPTY;

    /* chmod 755: чисто цифровая запись задаёт права целиком */
    if (all_digits(expr)) return perm_parse_octal(expr, mode);

    unsigned cur = *mode & MODE_MASK;
    const char *p = expr;

    for (;;) {                                  /* один цикл - одна команда между запятыми */
        unsigned who = 0;                       /* бит 0 - u, бит 1 - g, бит 2 - o */
        int has_who = 0;

        for (; *p == 'u' || *p == 'g' || *p == 'o' || *p == 'a'; p++) {
            has_who = 1;
            who |= (*p == 'u') ? 1u : (*p == 'g') ? 2u : (*p == 'o') ? 4u : 7u;
        }
        if (!has_who) who = 7;                  /* "+x" означает "a+x" с учётом umask */

        unsigned who_mask = 0;
        for (int cls = 0; cls < 3; cls++)
            if (who & (1u << cls)) who_mask |= class_mask(cls);

        if (*p != '+' && *p != '-' && *p != '=') return PERM_ERR_CHMOD;

        while (*p == '+' || *p == '-' || *p == '=') {
            char op = *p++;
            unsigned value = 0;

            if (*p == 'u' || *p == 'g' || *p == 'o') {
                /* копирование прав другого класса: g=u */
                int src = (*p == 'u') ? 0 : (*p == 'g') ? 1 : 2;
                p++;
                unsigned bits = (cur >> class_shift(src)) & 7u;
                for (int cls = 0; cls < 3; cls++)
                    if (who & (1u << cls)) value |= bits << class_shift(cls);
            } else {
                for (; *p == 'r' || *p == 'w' || *p == 'x' || *p == 's' || *p == 't'; p++) {
                    for (int cls = 0; cls < 3; cls++) {
                        if (!(who & (1u << cls))) continue;
                        if (*p == 'r') value |= 4u << class_shift(cls);
                        if (*p == 'w') value |= 2u << class_shift(cls);
                        if (*p == 'x') value |= 1u << class_shift(cls);
                    }
                    if (*p == 's') {
                        if (who & 1u) value |= BIT_SETUID;
                        if (who & 2u) value |= BIT_SETGID;
                    }
                    if (*p == 't' && (who & 4u)) value |= BIT_STICKY;
                }
            }

            if (!has_who) value &= ~(umask_value & 0777u);

            switch (op) {
            case '+': cur |= value; break;
            case '-': cur &= ~value; break;
            case '=': cur = (cur & ~who_mask) | value; break;
            }
        }

        if (*p == ',') { p++; continue; }       /* дальше должна идти ещё одна команда */
        if (*p == '\0') break;
        return PERM_ERR_CHMOD;                  /* неизвестный символ */
    }

    *mode = cur & MODE_MASK;
    return PERM_OK;
}

/* ---------- представления ---------- */

void perm_to_symbolic(unsigned mode, char out[PERM_SYM_SIZE])
{
    for (int cls = 0; cls < 3; cls++) {
        unsigned bits = (mode >> class_shift(cls)) & 7u;
        out[cls * 3 + 0] = (bits & 4u) ? 'r' : '-';
        out[cls * 3 + 1] = (bits & 2u) ? 'w' : '-';
        out[cls * 3 + 2] = (bits & 1u) ? 'x' : '-';
    }
    if (mode & BIT_SETUID) out[2] = (mode & 0100u) ? 's' : 'S';
    if (mode & BIT_SETGID) out[5] = (mode & 0010u) ? 's' : 'S';
    if (mode & BIT_STICKY) out[8] = (mode & 0001u) ? 't' : 'T';
    out[9] = '\0';
}

void perm_to_octal(unsigned mode, char out[PERM_OCT_SIZE])
{
    snprintf(out, PERM_OCT_SIZE, "%04o", mode & MODE_MASK);
}

void perm_to_binary(unsigned mode, char out[PERM_BIN_SIZE])
{
    size_t pos = 0;
    for (int bit = 11; bit >= 0; bit--) {
        out[pos++] = (mode & (1u << bit)) ? '1' : '0';
        if (bit % 3 == 0 && bit > 0) out[pos++] = ' ';   /* группы по три бита */
    }
    out[pos] = '\0';
}

/* ---------- файл ---------- */

int perm_stat(const char *path, FileInfo *info)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;

    info->mode = (unsigned)(st.st_mode & MODE_MASK);
    info->nlink = (unsigned long)st.st_nlink;
    info->uid = (unsigned long)st.st_uid;
    info->gid = (unsigned long)st.st_gid;
    info->size = (long long)st.st_size;

    /* В Windows (MinGW) некоторых макросов нет, поэтому проверяем каждый */
    info->type = '-';
    if (S_ISDIR(st.st_mode))           info->type = 'd';
#ifdef S_ISCHR
    else if (S_ISCHR(st.st_mode))      info->type = 'c';
#endif
#ifdef S_ISBLK
    else if (S_ISBLK(st.st_mode))      info->type = 'b';
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(st.st_mode))     info->type = 'p';
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(st.st_mode))     info->type = 's';
#endif
    return 0;
}

const char *perm_status_message(PermStatus status)
{
    switch (status) {
    case PERM_OK:               return "OK";
    case PERM_ERR_EMPTY:        return "Empty input.";
    case PERM_ERR_OCTAL_DIGIT:  return "Octal digits must be 0-7.";
    case PERM_ERR_LENGTH:       return "Octal mode has at most 4 digits.";
    case PERM_ERR_SYMBOLIC:     return "Invalid letter notation. Expected 9 characters like rwxr-xr-x.";
    case PERM_ERR_CHMOD:        return "Invalid command. Examples: u+x  g-w,o=r  a+rw  g=u  755";
    }
    return "Unknown error.";
}
