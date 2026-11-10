#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "pqueue.h"

#define DEFAULT_COUNT 12
#define MAX_COUNT 200
#define EXTRA_MESSAGES 8        /* запас под сообщения, добавляемые по ходу теста */

/* Сообщение, которое "отправляется" в очередь */
typedef struct {
    int id;
    char text[48];
} Message;

/* Эталонная модель: простой массив всех сгенерированных сообщений.
   Ожидаемый результат каждой выборки вычисляется здесь перебором "в лоб"
   прямо по определению и сравнивается с ответом очереди. */
typedef struct {
    int n;
    int capacity;
    int *priority;       /* priority[i] - приоритет сообщения с id = i + 1 */
    char *taken;         /* 1, если сообщение уже извлечено */
} Model;

static int checks_total = 0;
static int checks_failed = 0;

/* Приоритеты, которые встречаются чаще других, чтобы выборка "по приоритету"
   находила несколько сообщений с одним значением. */
static const int PALETTE[] = { 0, 10, 50, 100, 150, 200, 255 };
#define PALETTE_SIZE ((int)(sizeof PALETTE / sizeof PALETTE[0]))

/* =====================  ЭТАЛОННАЯ МОДЕЛЬ  ===================== */

/* Первый в очереди: наибольший приоритет, среди равных - раньше добавленный */
static int model_first(const Model *m)
{
    int best = -1;
    for (int i = 0; i < m->n; i++) {
        if (m->taken[i]) continue;
        if (best < 0 || m->priority[i] > m->priority[best]) best = i;   /* строго больше: старший id остаётся */
    }
    return best;
}

static int model_with_priority(const Model *m, int priority)
{
    for (int i = 0; i < m->n; i++)
        if (!m->taken[i] && m->priority[i] == priority) return i;
    return -1;
}

static int model_at_least(const Model *m, int min_priority)
{
    int first = model_first(m);
    return (first >= 0 && m->priority[first] >= min_priority) ? first : -1;
}

/* =====================  ГЕНЕРАЦИЯ СООБЩЕНИЙ  ===================== */

static int random_priority(void)
{
    if (rand() % 10 < 7) return PALETTE[rand() % PALETTE_SIZE];
    return rand() % (PQ_MAX_PRIORITY + 1);
}

/* Создаёт сообщение и кладёт его в очередь. priority < 0 - случайный приоритет. */
static int generate(PriorityQueue *pq, Model *m, int priority)
{
    if (m->n >= m->capacity) return 0;

    Message *msg = (Message *)malloc(sizeof(Message));
    if (!msg) return 0;

    int prio = (priority >= 0) ? priority : random_priority();
    msg->id = m->n + 1;
    snprintf(msg->text, sizeof msg->text, "message %d", msg->id);

    PqStatus st = pq_push(pq, prio, msg);
    if (st != PQ_OK) {
        printf("  push failed: %s\n", pq_status_message(st));
        free(msg);
        return 0;
    }

    m->priority[m->n] = prio;
    m->taken[m->n] = 0;
    m->n++;
    printf("  #%-3d priority=%-3d \"%s\"\n", msg->id, prio, msg->text);
    return 1;
}

/* =====================  ВЫБОРКА И ПРОВЕРКА  ===================== */

/* Печатает результат запроса, сверяет его с эталоном и освобождает сообщение.
   expected - индекс сообщения, которое должно прийти, или -1, если ничего не должно. */
static void finish(const char *request, PqStatus st, PqItem *item, int expected, Model *m)
{
    int ok;
    printf("  %-24s -> ", request);

    if (st == PQ_OK) {
        Message *msg = (Message *)item->data;
        printf("#%-3d priority=%-3d \"%s\"", msg->id, item->priority, msg->text);
        ok = (expected >= 0 && msg->id - 1 == expected && item->priority == m->priority[expected]);
        m->taken[msg->id - 1] = 1;          /* модель следует за очередью, чтобы тест продолжался */
        free(msg);
    } else {
        printf("%s", pq_status_message(st));
        ok = (st == PQ_NOT_FOUND && expected < 0);
    }

    checks_total++;
    if (!ok) checks_failed++;
    printf("   [%s]\n", ok ? "ok" : "FAIL");
}

static void test_pop_first(PriorityQueue *pq, Model *m)
{
    PqItem item = { 0, NULL };
    int expected = model_first(m);
    PqStatus st = pq_pop(pq, &item);
    finish("pop (first in queue)", st, &item, expected, m);
}

static void test_pop_priority(PriorityQueue *pq, Model *m, int priority)
{
    char label[40];
    PqItem item = { 0, NULL };
    snprintf(label, sizeof label, "pop_priority(%d)", priority);

    int expected = model_with_priority(m, priority);
    PqStatus st = pq_pop_priority(pq, priority, &item);
    finish(label, st, &item, expected, m);
}

static void test_pop_at_least(PriorityQueue *pq, Model *m, int min_priority)
{
    char label[40];
    PqItem item = { 0, NULL };
    snprintf(label, sizeof label, "pop_at_least(%d)", min_priority);

    int expected = model_at_least(m, min_priority);
    PqStatus st = pq_pop_at_least(pq, min_priority, &item);
    finish(label, st, &item, expected, m);
}

/* Проверяет, что функция вернула именно ожидаемый код ошибки */
static void expect_status(const char *request, PqStatus got, PqStatus want)
{
    int ok = (got == want);
    checks_total++;
    if (!ok) checks_failed++;
    printf("  %-24s -> %s   [%s]\n", request, pq_status_message(got), ok ? "ok" : "FAIL");
}

/* ---- вспомогательные обходы ---- */

static void print_item(int priority, void *data, void *ctx)
{
    int *position = (int *)ctx;
    Message *msg = (Message *)data;
    printf("  %3d. priority=%-3d #%-3d \"%s\"\n", ++*position, priority, msg->id, msg->text);
}

static void print_queue(const PriorityQueue *pq)
{
    int position = 0;
    if (pq_size(pq) == 0) {
        printf("  (empty)\n");
        return;
    }
    pq_for_each(pq, print_item, &position);
}

/* Приоритет, у которого в очереди больше всего сообщений (-1, если очередь пуста) */
static int most_populated_priority(const PriorityQueue *pq)
{
    int best = -1;
    size_t best_count = 0;
    for (int p = PQ_MIN_PRIORITY; p <= PQ_MAX_PRIORITY; p++) {
        size_t c = pq_count_priority(pq, p);
        if (c > best_count) { best_count = c; best = p; }
    }
    return best;
}

/* Приоритет, которого в очереди нет вообще (-1, если заняты все 256) */
static int unused_priority(const PriorityQueue *pq)
{
    for (int p = 1; p <= PQ_MAX_PRIORITY; p++)
        if (pq_count_priority(pq, p) == 0) return p;
    return -1;
}

/* =====================  ВВОД АРГУМЕНТОВ  ===================== */

/* Целое число из аргумента командной строки в диапазоне [min, max]. 0 - ошибка. */
static int parse_int(const char *text, long min, long max, long *out)
{
    if (text[0] == '\0') return 0;
    for (const char *p = text; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;

    errno = 0;
    char *end;
    long v = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || v < min || v > max) return 0;
    *out = v;
    return 1;
}

/* =====================  main  ===================== */

int main(int argc, char *argv[])
{
    long count = DEFAULT_COUNT;
    long seed = (long)time(NULL);

    if (argc > 3 ||
        (argc >= 2 && !parse_int(argv[1], 1, MAX_COUNT, &count)) ||
        (argc == 3 && !parse_int(argv[2], 0, 2147483647L, &seed))) {
        fprintf(stderr, "Usage: %s [count 1..%d] [seed]\n", argv[0], MAX_COUNT);
        return 1;
    }
    srand((unsigned)seed);

    PriorityQueue pq;
    pq_init(&pq);

    Model model;
    model.n = 0;
    model.capacity = (int)count + EXTRA_MESSAGES;
    model.priority = (int *)calloc((size_t)model.capacity, sizeof(int));
    model.taken = (char *)calloc((size_t)model.capacity, 1);
    if (!model.priority || !model.taken) {
        fprintf(stderr, "Out of memory.\n");
        free(model.priority);
        free(model.taken);
        return 1;
    }

    printf("Messages: %ld, seed: %ld\n", count, seed);

    /* ---- 1. генерация сообщений ---- */
    printf("\n--- 1. Generating messages (in arrival order) ---\n");
    for (long i = 0; i < count; i++)
        generate(&pq, &model, -1);

    printf("\n--- 2. Queue content (in extraction order) ---\n");
    print_queue(&pq);

    /* ---- 3. первый в очереди ---- */
    printf("\n--- 3. Extract the first in queue ---\n");
    for (int i = 0; i < 3; i++)
        test_pop_first(&pq, &model);

    print_queue(&pq);

    /* ---- 4. по указанному приоритету ---- */
    printf("\n--- 4. Extract with a given priority ---\n");
    int busy = most_populated_priority(&pq);
    if (busy >= 0) {
        test_pop_priority(&pq, &model, busy);       /* самое старое сообщение этого приоритета */
        test_pop_priority(&pq, &model, busy);       /* следующее по возрасту */
    }
    int absent = unused_priority(&pq);
    if (absent >= 0)
        test_pop_priority(&pq, &model, absent);     /* такого приоритета в очереди нет */

    print_queue(&pq);

    /* ---- 5. приоритет не ниже заданного ---- */
    printf("\n--- 5. Extract with priority not lower than a threshold ---\n");
    test_pop_at_least(&pq, &model, 200);
    test_pop_at_least(&pq, &model, 100);
    test_pop_at_least(&pq, &model, 0);
    PqItem top;
    if (pq_peek(&pq, &top) == PQ_OK && top.priority < PQ_MAX_PRIORITY)
        test_pop_at_least(&pq, &model, top.priority + 1);   /* выше текущего максимума: ничего нет */


    print_queue(&pq);

    /* ---- 6. срочное сообщение обгоняет остальных ---- */
    printf("\n--- 6. A new urgent message jumps ahead of the queue ---\n");
    generate(&pq, &model, 255);
    test_pop_first(&pq, &model);

    /* ---- 7. неверные приоритеты ---- */
    printf("\n--- 7. Invalid priorities ---\n");
    expect_status("push(priority = 256)", pq_push(&pq, 256, NULL), PQ_ERR_PRIORITY);
    expect_status("push(priority = -1)", pq_push(&pq, -1, NULL), PQ_ERR_PRIORITY);
    expect_status("pop_priority(300)", pq_pop_priority(&pq, 300, &top), PQ_ERR_PRIORITY);
    expect_status("pop_at_least(-5)", pq_pop_at_least(&pq, -5, &top), PQ_ERR_PRIORITY);

    /* ---- 8. остаток очереди ---- */
    printf("\n--- 8. Drain the rest (%zu left) ---\n", pq_size(&pq));
    while (model_first(&model) >= 0)
        test_pop_first(&pq, &model);

    expect_status("pop on empty queue", pq_pop(&pq, &top), PQ_NOT_FOUND);
    expect_status("pop_at_least(0) on empty", pq_pop_at_least(&pq, 0, &top), PQ_NOT_FOUND);
    checks_total++;
    if (pq_size(&pq) != 0) {
        checks_failed++;
        printf("  queue size is %zu, expected 0   [FAIL]\n", pq_size(&pq));
    }

    printf("\nChecks: %d passed, %d failed.\n", checks_total - checks_failed, checks_failed);

    pq_clear(&pq, free);
    free(model.priority);
    free(model.taken);
    return checks_failed == 0 ? 0 : 1;
}
