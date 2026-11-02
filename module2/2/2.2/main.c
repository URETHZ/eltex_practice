#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include "calculator.h"

#define BUF_SIZE 128

/* =====================  ВВОД И ПРОВЕРКА  ===================== */

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

/* Запрашивает вещественное число, пока ввод не станет корректным. 0 - EOF.
   Десятичная запятая допускается наравне с точкой. */
static int read_double(const char *prompt, double *out)
{
    char buf[BUF_SIZE];
    for (;;) {
        if (!read_line(prompt, buf, sizeof buf)) return 0;

        for (char *p = buf; *p; p++)
            if (*p == ',') *p = '.';

        char *end;
        errno = 0;
        double v = strtod(buf, &end);
        if (buf[0] != '\0' && *end == '\0' && errno != ERANGE && isfinite(v)) {
            *out = v;
            return 1;
        }
        printf("Invalid number, try again.\n");
    }
}

/* Запрашивает целое число, пока ввод не станет корректным. 0 - EOF. */
static int read_long(const char *prompt, long *out)
{
    char buf[BUF_SIZE];
    for (;;) {
        if (!read_line(prompt, buf, sizeof buf)) return 0;

        char *end;
        errno = 0;
        long v = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && errno != ERANGE) {
            *out = v;
            return 1;
        }
        printf("Invalid integer, try again.\n");
    }
}

/* Запрашивает номер пункта меню из [min, max]. 0 - EOF. */
static int read_choice(const char *prompt, long min, long max, long *out)
{
    for (;;) {
        if (!read_long(prompt, out)) return 0;
        if (*out >= min && *out <= max) return 1;
        printf("Enter a number from %ld to %ld.\n", min, max);
    }
}

/* =====================  ОПЕРАЦИИ МЕНЮ  ===================== */

typedef CalcStatus (*BinaryOp)(double, double, double *);

static void print_status_error(CalcStatus st)
{
    printf("Error: %s\n", calc_status_message(st));
}

/* Общая обработка бинарных операций над вещественными числами. */
static int run_binary(const char *symbol, BinaryOp op)
{
    double a, b, r;
    if (!read_double("First argument: ", &a)) return 0;
    if (!read_double("Second argument: ", &b)) return 0;

    CalcStatus st = op(a, b, &r);
    if (st == CALC_OK) printf("%.10g %s %.10g = %.10g\n", a, symbol, b, r);
    else               print_status_error(st);
    return 1;
}

static int run_sqrt(void)
{
    double a, r;
    if (!read_double("Argument: ", &a)) return 0;

    CalcStatus st = calc_sqrt(a, &r);
    if (st == CALC_OK) printf("sqrt(%.10g) = %.10g\n", a, r);
    else               print_status_error(st);
    return 1;
}

static int run_factorial(void)
{
    long n;
    double r;
    if (!read_long("Integer n: ", &n)) return 0;

    CalcStatus st = calc_factorial(n, &r);
    if (st == CALC_OK) printf("%ld! = %.15g\n", n, r);
    else               print_status_error(st);
    return 1;
}

static int run_mod(void)
{
    long a, b, r;
    if (!read_long("Dividend (integer): ", &a)) return 0;
    if (!read_long("Divisor (integer): ", &b)) return 0;

    CalcStatus st = calc_mod(a, b, &r);
    if (st == CALC_OK) printf("%ld %% %ld = %ld\n", a, b, r);
    else               print_status_error(st);
    return 1;
}

static void print_menu(void)
{
    printf("\n=== Calculator ===\n"
           "1) Addition        (a + b)\n"
           "2) Subtraction     (a - b)\n"
           "3) Multiplication  (a * b)\n"
           "4) Division        (a / b)\n"
           "5) Power           (a ^ b)\n"
           "6) Square root\n"
           "7) Factorial\n"
           "8) Remainder       (a %% b, integers)\n"
           "0) Exit\n");
}

/* =====================  main  ===================== */

int main(void)
{
    #ifdef _WIN32
        system("chcp 65001 > nul");
    #endif
    for (;;) {
        print_menu();

        long choice;
        if (!read_choice("Action: ", 0, 8, &choice) || choice == 0) break;

        int ok = 1;                     /* 0 - ввод оборвался (EOF) */
        switch (choice) {
        case 1: ok = run_binary("+", calc_add); break;
        case 2: ok = run_binary("-", calc_sub); break;
        case 3: ok = run_binary("*", calc_mul); break;
        case 4: ok = run_binary("/", calc_div); break;
        case 5: ok = run_binary("^", calc_pow); break;
        case 6: ok = run_sqrt(); break;
        case 7: ok = run_factorial(); break;
        case 8: ok = run_mod(); break;
        }
        if (!ok) break;
    }

    printf("Bye.\n");
    return 0;
}
