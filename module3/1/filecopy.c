#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>

#define BUFSIZE 4096
#define READY_BYTE 'R'

/* Заголовок, передаваемый перед каждым файлом.
 * name_len == 0 означает завершение работы. */
struct file_header {
    uint32_t name_len;
    uint64_t file_size;
};

/* Полная запись count байт из buf в fd.
 * Возвращает 0 при успехе, -1 при ошибке. */
static int write_full(int fd, const void *buf, size_t count) {
    const char *p = buf;
    while (count > 0) {
        ssize_t n = write(fd, p, count);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        p += n;
        count -= n;
    }
    return 0;
}

/* Полное чтение count байт в buf из fd.
 * Возвращает 0 при успехе, -1 при ошибке/EOF. */
static int read_full(int fd, void *buf, size_t count) {
    char *p = buf;
    while (count > 0) {
        ssize_t n = read(fd, p, count);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1; /* преждевременный EOF */
        p += n;
        count -= n;
    }
    return 0;
}

/* Создание имён для именованных каналов на основе базового имени. */
static void make_fifo_names(const char *base, char **data_name, char **sync_name) {
    size_t len = strlen(base);
    *data_name = malloc(len + 6); // "_data" + '\0'
    *sync_name = malloc(len + 6); // "_sync" + '\0'
    if (!*data_name || !*sync_name) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    sprintf(*data_name, "%s_data", base);
    sprintf(*sync_name, "%s_sync", base);
}

/* Удаление именованных каналов (игнорируем ошибки). */
static void unlink_fifos(const char *data_name, const char *sync_name) {
    unlink(data_name);
    unlink(sync_name);
}

int main(int argc, char *argv[]) {
    char *pipe_base = NULL;          // имя, заданное -p
    char *data_fifo = NULL;          // имя канала данных (если -p)
    char *sync_fifo = NULL;          // имя канала синхронизации (если -p)
    int opt;

    /* Разбор аргументов: -p <имя> */
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                pipe_base = optarg;
                break;
            default:
                fprintf(stderr, "Использование: %s [-p <имя_канала>] файл1 [файл2 ...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    int file_count = argc - optind;
    if (file_count <= 0) {
        fprintf(stderr, "Ошибка: не указаны файлы для копирования.\n");
        exit(EXIT_FAILURE);
    }

    char **files = &argv[optind];

    /* Дескрипторы каналов */
    int data_pipe[2];   // [0] чтение, [1] запись
    int sync_pipe[2];   // [0] чтение, [1] запись

    if (pipe_base) {
        /* Именованные каналы */
        make_fifo_names(pipe_base, &data_fifo, &sync_fifo);

        /* Удаляем старые, если остались */
        unlink_fifos(data_fifo, sync_fifo);

        if (mkfifo(data_fifo, 0666) < 0) {
            perror("mkfifo data");
            exit(EXIT_FAILURE);
        }
        if (mkfifo(sync_fifo, 0666) < 0) {
            perror("mkfifo sync");
            unlink(data_fifo);
            exit(EXIT_FAILURE);
        }

        /* Открываем каналы (порядок важен для избежания взаимной блокировки) */
        /* Родитель: data -> запись, sync -> чтение.
         * Ребёнок: data -> чтение, sync -> запись.
         * После fork родитель откроет data на запись (блокируется),
         * затем sync на чтение (блокируется).
         * Ребёнок откроет data на чтение (разблокирует родителя),
         * затем sync на запись (разблокирует родителя). */
    } else {
        /* Безымянные каналы */
        if (pipe(data_pipe) < 0) {
            perror("pipe data");
            exit(EXIT_FAILURE);
        }
        if (pipe(sync_pipe) < 0) {
            perror("pipe sync");
            close(data_pipe[0]);
            close(data_pipe[1]);
            exit(EXIT_FAILURE);
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        /* ---------- ДОЧЕРНИЙ ПРОЦЕСС ---------- */
        int data_fd, sync_fd;

        if (pipe_base) {
            /* Закрываем ненужные концы (для именованных каналов не применимо) */
            /* Открываем именованные каналы */
            data_fd = open(data_fifo, O_RDONLY);
            if (data_fd < 0) {
                perror("open data_fifo (child)");
                exit(EXIT_FAILURE);
            }
            sync_fd = open(sync_fifo, O_WRONLY);
            if (sync_fd < 0) {
                perror("open sync_fifo (child)");
                close(data_fd);
                exit(EXIT_FAILURE);
            }
        } else {
            /* Безымянные каналы: закрываем ненужные концы */
            close(data_pipe[1]); // запись не нужна
            close(sync_pipe[0]); // чтение не нужно
            data_fd = data_pipe[0];
            sync_fd = sync_pipe[1];
        }

        for (;;) {
            /* 1. Сообщаем о готовности */
            if (write_full(sync_fd, &(char){READY_BYTE}, 1) < 0) {
                perror("write ready");
                exit(EXIT_FAILURE);
            }

            /* 2. Читаем заголовок */
            struct file_header hdr;
            if (read_full(data_fd, &hdr, sizeof(hdr)) < 0) {
                perror("read header");
                exit(EXIT_FAILURE);
            }

            if (hdr.name_len == 0) {
                /* Сигнал завершения */
                break;
            }

            /* 3. Читаем имя файла */
            char *name = malloc(hdr.name_len + 1);
            if (!name) {
                perror("malloc name");
                exit(EXIT_FAILURE);
            }
            if (read_full(data_fd, name, hdr.name_len) < 0) {
                perror("read filename");
                free(name);
                exit(EXIT_FAILURE);
            }
            name[hdr.name_len] = '\0';

            /* 4. Создаём файл .copy */
            char *copy_name = malloc(strlen(name) + 6); // ".copy" + '\0'
            if (!copy_name) {
                perror("malloc copy_name");
                free(name);
                exit(EXIT_FAILURE);
            }
            sprintf(copy_name, "%s.copy", name);

            int out_fd = open(copy_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror("open copy file");
                free(name);
                free(copy_name);
                exit(EXIT_FAILURE);
            }

            /* 5. Принимаем данные блоками */
            uint64_t remaining = hdr.file_size;
            char buf[BUFSIZE];
            while (remaining > 0) {
                size_t to_read = (remaining < BUFSIZE) ? remaining : BUFSIZE;
                ssize_t n = read(data_fd, buf, to_read);
                if (n <= 0) {
                    if (n < 0 && errno == EINTR) continue;
                    perror("read data");
                    close(out_fd);
                    free(name);
                    free(copy_name);
                    exit(EXIT_FAILURE);
                }
                if (write_full(out_fd, buf, n) < 0) {
                    perror("write copy");
                    close(out_fd);
                    free(name);
                    free(copy_name);
                    exit(EXIT_FAILURE);
                }
                remaining -= n;
            }

            close(out_fd);
            free(name);
            free(copy_name);
        }

        /* Закрываем каналы */
        close(data_fd);
        close(sync_fd);
        exit(EXIT_SUCCESS);

    } else {
        /* ---------- РОДИТЕЛЬСКИЙ ПРОЦЕСС ---------- */
        int data_fd, sync_fd;

        if (pipe_base) {
            /* Открываем именованные каналы */
            data_fd = open(data_fifo, O_WRONLY);
            if (data_fd < 0) {
                perror("open data_fifo (parent)");
                exit(EXIT_FAILURE);
            }
            sync_fd = open(sync_fifo, O_RDONLY);
            if (sync_fd < 0) {
                perror("open sync_fifo (parent)");
                close(data_fd);
                exit(EXIT_FAILURE);
            }
        } else {
            /* Безымянные каналы: закрываем ненужные концы */
            close(data_pipe[0]); // чтение не нужно
            close(sync_pipe[1]); // запись не нужна
            data_fd = data_pipe[1];
            sync_fd = sync_pipe[0];
        }

        for (int i = 0; i < file_count; i++) {
            const char *fname = files[i];

            /* Открываем исходный файл */
            int in_fd = open(fname, O_RDONLY);
            if (in_fd < 0) {
                fprintf(stderr, "Ошибка: не удалось открыть '%s': %s\n",
                        fname, strerror(errno));
                continue; // пропускаем этот файл
            }

            /* Получаем размер файла */
            struct stat st;
            if (fstat(in_fd, &st) < 0) {
                perror("fstat");
                close(in_fd);
                continue;
            }
            uint64_t file_size = (uint64_t)st.st_size;

            /* Ждём готовности от ребёнка */
            char ready;
            if (read_full(sync_fd, &ready, 1) < 0 || ready != READY_BYTE) {
                fprintf(stderr, "Ошибка: не получен сигнал готовности от ребёнка.\n");
                close(in_fd);
                break;
            }

            /* Формируем и отправляем заголовок */
            struct file_header hdr;
            hdr.name_len = (uint32_t)strlen(fname);
            hdr.file_size = file_size;

            if (write_full(data_fd, &hdr, sizeof(hdr)) < 0 ||
                write_full(data_fd, fname, hdr.name_len) < 0) {
                perror("write header");
                close(in_fd);
                break;
            }

            /* Передаём содержимое файла блоками */
            char buf[BUFSIZE];
            ssize_t n;
            while ((n = read(in_fd, buf, BUFSIZE)) > 0) {
                if (write_full(data_fd, buf, n) < 0) {
                    perror("write data");
                    close(in_fd);
                    goto cleanup;
                }
            }
            if (n < 0) {
                perror("read source");
                close(in_fd);
                goto cleanup;
            }
            close(in_fd);
        }

        /* Сигнал завершения: заголовок с name_len == 0 */
        struct file_header end_hdr = { .name_len = 0, .file_size = 0 };
        write_full(data_fd, &end_hdr, sizeof(end_hdr));

    cleanup:
        /* Закрываем каналы */
        close(data_fd);
        close(sync_fd);

        /* Ждём завершения ребёнка */
        int status;
        waitpid(pid, &status, 0);

        /* Удаляем именованные каналы, если использовались */
        if (pipe_base) {
            unlink_fifos(data_fifo, sync_fifo);
            free(data_fifo);
            free(sync_fifo);
        }

        return 0;
    }
}
