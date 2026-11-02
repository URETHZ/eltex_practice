#ifndef PERMS_H
#define PERMS_H

/* Размеры буферов для текстовых представлений (с учётом '\0') */
#define PERM_SYM_SIZE 10   /* "rwxr-xr-x"        */
#define PERM_OCT_SIZE 5    /* "0755"             */
#define PERM_BIN_SIZE 16   /* "000 111 101 101"  */

/* Маска прав хранится в unsigned (12 бит):
     биты 11..9 - setuid, setgid, sticky
     биты 8..6  - права владельца (r w x)
     биты 5..3  - права группы    (r w x)
     биты 2..0  - права остальных (r w x)      */

typedef enum {
    PERM_OK = 0,
    PERM_ERR_EMPTY,        /* пустая строка */
    PERM_ERR_OCTAL_DIGIT,  /* в восьмеричной записи встретилась цифра 8 или 9 */
    PERM_ERR_LENGTH,       /* в восьмеричной записи больше 4 цифр */
    PERM_ERR_SYMBOLIC,     /* неверная буквенная запись, например "rwxr-xr-x" */
    PERM_ERR_CHMOD         /* неверная команда модификации */
} PermStatus;

/* Сведения о файле, полученные через stat */
typedef struct {
    unsigned mode;         /* 12 бит прав */
    char type;             /* '-', 'd', 'c', 'b', 'p', 's' - как в первом символе ls -l */
    unsigned long nlink;
    unsigned long uid;
    unsigned long gid;
    long long size;
} FileInfo;

/* ---- Разбор ---- */
/* Автоматически определяет запись: только цифры - восьмеричная, иначе буквенная. */
PermStatus perm_parse(const char *text, unsigned *mode);
PermStatus perm_parse_octal(const char *text, unsigned *mode);      /* "755", "4755", "0644" */
PermStatus perm_parse_symbolic(const char *text, unsigned *mode);   /* "rwxr-xr-x" или "-rwxr-xr-x" */

/* ---- Модификация, подобная chmod ----
   expr: "u+x,g-w,o=r", "a+rw", "+x", "g=u", "u+s" или просто "755".
   umask_value учитывается, когда в команде не указано, для кого (как в chmod).
   При ошибке *mode не изменяется. */
PermStatus perm_apply_chmod(unsigned *mode, const char *expr, unsigned umask_value);

/* ---- Представления ---- */
void perm_to_symbolic(unsigned mode, char out[PERM_SYM_SIZE]);
void perm_to_octal(unsigned mode, char out[PERM_OCT_SIZE]);
void perm_to_binary(unsigned mode, char out[PERM_BIN_SIZE]);

/* ---- Файл ---- */
/* 0 - успех, -1 - ошибка (причина в errno) */
int perm_stat(const char *path, FileInfo *info);

const char *perm_status_message(PermStatus status);

#endif /* PERMS_H */
