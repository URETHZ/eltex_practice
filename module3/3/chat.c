/* P2P-чат на двух очередях сообщений POSIX.

   Запуск: ./chat <имя>        (например, ./chat /room)

   К имени добавляются суффиксы: <имя>_1 и <имя>_2.
     - Если очередей ещё нет, процесс создаёт обе. Он принимает сообщения через
       первую очередь (_1) и отправляет через вторую (_2). Создатель удаляет очереди.
     - Если очереди уже есть, процесс отправляет через первую (_1) и принимает
       через вторую (_2).

   Конец обмена передаётся сообщением с приоритетом PRIO_END (у обычных сообщений
   приоритет PRIO_TEXT). Оно отправляется по команде /quit, по Ctrl+D и по SIGINT. */

#define _GNU_SOURCE             /* ppoll() */

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_MESSAGES 10         /* сколько сообщений может ждать в очереди */
#define MSG_SIZE     256        /* максимальный размер одного сообщения в байтах */
#define PRIO_TEXT    1          /* приоритет обычного сообщения */
#define PRIO_END     10         /* приоритет сообщения об окончании работы (известен обеим сторонам) */
#define BASE_NAME_MAX 200

typedef struct {
    char name1[BASE_NAME_MAX + 8];
    char name2[BASE_NAME_MAX + 8];
    int creator;                /* 1 - этот процесс создал очереди и обязан их удалить */
    mqd_t recv_q;               /* очередь, из которой принимаем */
    mqd_t send_q;               /* очередь, в которую отправляем */
    size_t max_text;            /* сколько байтов текста помещается в одно сообщение */
    char *recv_buf;             /* буфер для mq_receive (размер сообщения + 1 для '\0') */
    size_t recv_size;
    char *send_buf;
} Chat;

/* Сигнал только поднимает флаг: всё остальное делается в основном цикле */
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ---------- имена очередей ---------- */

/* "/room" и "room" дают "/room_1" и "/room_2". Имя очереди POSIX начинается с '/'
   и больше слэшей не содержит. */
static int build_names(Chat *c, const char *arg)
{
    const char *base = (arg[0] == '/') ? arg + 1 : arg;

    if (base[0] == '\0' || strchr(base, '/') != NULL || strlen(base) > BASE_NAME_MAX)
        return -1;
    snprintf(c->name1, sizeof c->name1, "/%s_1", base);
    snprintf(c->name2, sizeof c->name2, "/%s_2", base);
    return 0;
}

/* ---------- открытие очередей ---------- */

static int open_queues(Chat *c)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof attr);
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MSG_SIZE;

    /* O_EXCL: создание удастся, только если очереди ещё нет. Так проверяем,
       первый ли мы. O_NONBLOCK: приём и отправка не блокируют процесс. */
    mqd_t q1 = mq_open(c->name1, O_CREAT | O_EXCL | O_RDONLY | O_NONBLOCK, 0600, &attr);

    if (q1 != (mqd_t)-1) {
        /* Очередей не было: создаём вторую, принимаем через первую, отправляем через вторую */
        mqd_t q2 = mq_open(c->name2, O_CREAT | O_EXCL | O_WRONLY | O_NONBLOCK, 0600, &attr);
        if (q2 == (mqd_t)-1) {
            fprintf(stderr, "chat: cannot create queue %s: %s\n", c->name2, strerror(errno));
            mq_close(q1);
            mq_unlink(c->name1);
            return -1;
        }
        c->creator = 1;
        c->recv_q = q1;
        c->send_q = q2;
        return 0;
    }

    if (errno != EEXIST) {
        fprintf(stderr, "chat: cannot create queue %s: %s\n", c->name1, strerror(errno));
        return -1;
    }

    /* Очереди уже есть: отправляем через первую, принимаем через вторую */
    c->creator = 0;
    c->send_q = mq_open(c->name1, O_WRONLY | O_NONBLOCK);
    if (c->send_q == (mqd_t)-1) {
        fprintf(stderr, "chat: cannot open queue %s: %s\n", c->name1, strerror(errno));
        return -1;
    }

    /* Создатель мог ещё не успеть создать вторую очередь - немного подождём */
    c->recv_q = (mqd_t)-1;
    for (int attempt = 0; attempt < 50; attempt++) {
        c->recv_q = mq_open(c->name2, O_RDONLY | O_NONBLOCK);
        if (c->recv_q != (mqd_t)-1 || errno != ENOENT) break;
        usleep(20 * 1000);
    }
    if (c->recv_q == (mqd_t)-1) {
        fprintf(stderr, "chat: cannot open queue %s: %s\n", c->name2, strerror(errno));
        mq_close(c->send_q);
        return -1;
    }
    return 0;
}

/* Размеры берём у реально открытых очередей: они могли быть созданы с другими параметрами */
static int prepare_buffers(Chat *c)
{
    struct mq_attr in, out;
    if (mq_getattr(c->recv_q, &in) == -1 || mq_getattr(c->send_q, &out) == -1) {
        perror("chat: mq_getattr");
        return -1;
    }
    if (out.mq_msgsize < 2) {
        fprintf(stderr, "chat: the send queue message size is too small\n");
        return -1;
    }

    c->recv_size = (size_t)in.mq_msgsize;
    c->max_text = (size_t)out.mq_msgsize - 1;           /* один байт под '\0' */
    c->recv_buf = (char *)malloc(c->recv_size + 1);
    c->send_buf = (char *)malloc(c->max_text + 1);
    if (!c->recv_buf || !c->send_buf) {
        fprintf(stderr, "chat: out of memory\n");
        return -1;
    }
    return 0;
}

/* ---------- приём ---------- */

/* Забирает из очереди все сообщения, которые там есть. Сообщение с приоритетом
   PRIO_END означает, что собеседник закончил работу: он будет первым в очереди,
   но остальные сообщения всё равно выводим, чтобы ничего не потерять. */
static void drain_incoming(Chat *c, int *peer_done)
{
    for (;;) {
        unsigned prio;
        ssize_t n = mq_receive(c->recv_q, c->recv_buf, c->recv_size, &prio);

        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno != EAGAIN) perror("chat: mq_receive");     /* EAGAIN - очередь пуста */
            return;
        }
        c->recv_buf[n] = '\0';

        if (prio == PRIO_END) {
            *peer_done = 1;
        } else {
            printf("peer> %s\n", c->recv_buf);
            fflush(stdout);
        }
    }
}

/* ---------- отправка ---------- */

static void send_text(Chat *c, const char *line, size_t len)
{
    while (len > 0) {
        size_t chunk = len < c->max_text ? len : c->max_text;

        /* Длинную строку делим на части, но не посреди многобайтового символа UTF-8 */
        if (chunk < len) {
            size_t back = chunk;
            while (back > 0 && ((unsigned char)line[back] & 0xC0) == 0x80) back--;
            if (back > 0) chunk = back;
        }

        memcpy(c->send_buf, line, chunk);
        c->send_buf[chunk] = '\0';

        if (mq_send(c->send_q, c->send_buf, chunk + 1, PRIO_TEXT) == -1) {
            if (errno == EAGAIN) printf("(the peer's queue is full, the message was not sent)\n");
            else                 perror("chat: mq_send");
            fflush(stdout);
            return;
        }
        line += chunk;
        len -= chunk;
    }
}

/* Уведомляет собеседника об окончании работы сообщением с приоритетом PRIO_END.
   Очередь открыта в неблокирующем режиме, поэтому для этого единственного
   сообщения переключаем её в блокирующий и ждём место не дольше секунды. */
static void notify_end(Chat *c)
{
    struct mq_attr attr;
    if (mq_getattr(c->send_q, &attr) == 0) {
        attr.mq_flags = 0;
        mq_setattr(c->send_q, &attr, NULL);
    }

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 1;

    if (mq_timedsend(c->send_q, "bye", 4, PRIO_END, &deadline) == -1)
        fprintf(stderr, "chat: could not notify the peer: %s\n", strerror(errno));
}

/* ---------- ввод пользователя ---------- */

/* Обрабатывает одну введённую строку. Возвращает 1, если пользователь хочет выйти. */
static int handle_line(Chat *c, char *line, size_t len)
{
    if (len > 0 && line[len - 1] == '\r') len--;
    if (len == 0) return 0;
    if (len == 5 && memcmp(line, "/quit", 5) == 0) return 1;

    send_text(c, line, len);
    return 0;
}

/* Читает stdin через read(), а не через stdio: poll() не видит данные, которые
   уже прочитал в свой буфер fgets(). Возвращает 1, если нужно выйти (/quit или конец ввода). */
static int read_input(Chat *c, char *buf, size_t *len, size_t cap)
{
    ssize_t n = read(STDIN_FILENO, buf + *len, cap - *len);

    if (n == 0) return 1;                               /* Ctrl+D */
    if (n < 0) return (errno == EINTR || errno == EAGAIN) ? 0 : 1;
    *len += (size_t)n;

    size_t start = 0;
    for (size_t i = 0; i < *len; i++) {
        if (buf[i] != '\n') continue;
        if (handle_line(c, buf + start, i - start)) return 1;
        start = i + 1;
    }

    if (start > 0) {                                    /* убираем обработанные строки */
        memmove(buf, buf + start, *len - start);
        *len -= start;
    }
    if (*len == cap) {                                  /* строка без перевода строки не помещается */
        int quit = handle_line(c, buf, *len);
        *len = 0;
        return quit;
    }
    return 0;
}

/* ---------- завершение ---------- */

static void cleanup(Chat *c)
{
    mq_close(c->recv_q);
    mq_close(c->send_q);
    if (c->creator) {                   /* очереди удаляет тот, кто их создал */
        mq_unlink(c->name1);
        mq_unlink(c->name2);
    }
    free(c->recv_buf);
    free(c->send_buf);
}

int main(int argc, char *argv[])
{
    Chat c;
    memset(&c, 0, sizeof c);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <queue name>\n"
                        "  Uses queues <name>_1 and <name>_2, e.g. %s /room\n"
                        "  Type /quit, press Ctrl+D or Ctrl+C to finish.\n", argv[0], argv[0]);
        return 2;
    }
    if (build_names(&c, argv[1]) != 0) {
        fprintf(stderr, "chat: invalid queue name '%s' (no '/' inside, at most %d characters)\n",
                argv[1], BASE_NAME_MAX);
        return 2;
    }

    /* SIGINT и SIGTERM только поднимают флаг. Пока основной цикл не ждёт в ppoll(),
       сигналы заблокированы - так флаг нельзя пропустить между проверкой и ожиданием. */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sigset_t blocked, original;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    sigprocmask(SIG_BLOCK, &blocked, &original);

    if (open_queues(&c) != 0) return 1;
    if (prepare_buffers(&c) != 0) {
        cleanup(&c);
        return 1;
    }

    if (c.creator)
        printf("Created queues %s and %s. Receiving via %s, sending via %s. Waiting for the peer.\n",
               c.name1, c.name2, c.name1, c.name2);
    else
        printf("Queues %s and %s already exist. Sending via %s, receiving via %s.\n",
               c.name1, c.name2, c.name1, c.name2);
    printf("Type a message and press Enter. /quit, Ctrl+D or Ctrl+C ends the chat.\n");
    fflush(stdout);

    char input[1024];
    size_t input_len = 0;
    int peer_done = 0;          /* собеседник прислал сообщение об окончании */
    int we_quit = 0;            /* мы сами решили закончить */
    int status = 0;

    while (!peer_done && !we_quit) {
        if (g_stop) {
            printf("\nSignal received, ending the chat.\n");
            we_quit = 1;
            break;
        }

        /* Асинхронное ожидание: и ввода пользователя, и сообщений от собеседника.
           В Linux дескриптор очереди сообщений можно передавать в poll(). */
        struct pollfd fds[2] = {
            { STDIN_FILENO,    POLLIN, 0 },
            { (int)c.recv_q,   POLLIN, 0 }
        };

        if (ppoll(fds, 2, NULL, &original) < 0) {       /* на время ожидания сигналы разблокируются */
            if (errno == EINTR) continue;               /* пришёл сигнал: флаг проверим в начале цикла */
            perror("chat: ppoll");
            status = 1;
            break;
        }

        if (fds[1].revents & POLLIN)
            drain_incoming(&c, &peer_done);

        if (!peer_done && (fds[0].revents & (POLLIN | POLLHUP))) {
            if (read_input(&c, input, &input_len, sizeof input))
                we_quit = 1;
        }
    }

    if (peer_done) {
        printf("The peer has finished the chat.\n");
    } else if (we_quit) {
        notify_end(&c);                                 /* сообщаем собеседнику, что уходим */
    }
    fflush(stdout);

    cleanup(&c);
    return status;
}
