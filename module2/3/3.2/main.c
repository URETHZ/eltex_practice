#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "ipv4.h"

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-v] <gateway> <mask> <N> [seed]\n"
            "  gateway  IPv4 address of the gateway, e.g. 192.168.1.1\n"
            "  mask     subnet mask: 255.255.255.0, 24 or /24\n"
            "  N        number of packets to simulate (positive integer)\n"
            "  seed     optional seed for the random generator (to repeat a run)\n"
            "  -v       print the decision for every packet\n"
            "Example: %s -v 192.168.1.1 255.255.255.0 10\n",
            prog, prog);
}

/* Разбор неотрицательного целого из аргумента (allow_zero - допускать ли 0). 0 - ошибка. */
static int parse_number(const char *text, int allow_zero, unsigned long long *out)
{
    if (text[0] == '\0') return 0;
    for (const char *p = text; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;      /* отвергает "-5", "+5", "1e3", "12abc" */

    errno = 0;
    char *end;
    unsigned long long v = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || (v == 0 && !allow_zero)) return 0;
    *out = v;
    return 1;
}

/* Случайный октет 0..255. Берём старшие биты rand(): они случайнее младших,
   а RAND_MAX может быть мал (например, 32767), поэтому 32 бита за один раз не взять. */
static unsigned random_octet(void)
{
    return (unsigned)((double)rand() / ((double)RAND_MAX + 1.0) * 256.0);
}

/* Случайное 32-битное число - адрес назначения пакета */
static uint32_t random_address(void)
{
    uint32_t ip = 0;
    for (int i = 0; i < 4; i++)
        ip = (ip << 8) | random_octet();
    return ip;
}

int main(int argc, char *argv[])
{
    /* ---- разбор аргументов: необязательный -v и 3-4 позиционных ---- */
    int verbose = 0;
    const char *pos[4];
    int npos = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (npos < 4) {
            pos[npos++] = argv[i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }
    if (npos < 3) {
        print_usage(argv[0]);
        return 1;
    }

    /* ---- проверка аргументов ---- */
    uint32_t gateway, mask;
    unsigned long long n, seed_value;
    Ipv4Status st;

    st = ipv4_parse(pos[0], &gateway);
    if (st != IPV4_OK) {
        fprintf(stderr, "Error: gateway '%s': %s\n", pos[0], ipv4_status_message(st));
        return 1;
    }
    st = ipv4_parse_mask(pos[1], &mask);
    if (st != IPV4_OK) {
        if (st == IPV4_ERR_FORMAT)
            fprintf(stderr, "Error: mask '%s': expected a.b.c.d (e.g. 255.255.255.0) or a prefix length (24 or /24)\n", pos[1]);
        else
            fprintf(stderr, "Error: mask '%s': %s\n", pos[1], ipv4_status_message(st));
        return 1;
    }
    if (!parse_number(pos[2], 0, &n)) {
        fprintf(stderr, "Error: N '%s' must be a positive integer.\n", pos[2]);
        return 1;
    }

    unsigned seed = (unsigned)time(NULL);
    if (npos == 4) {
        if (!parse_number(pos[3], 1, &seed_value)) {
            fprintf(stderr, "Error: seed '%s' must be a non-negative integer.\n", pos[3]);
            return 1;
        }
        seed = (unsigned)seed_value;
    }
    srand(seed);

    /* ---- описание сети ---- */
    char gw_str[IPV4_STR_SIZE], mask_str[IPV4_STR_SIZE];
    char net_str[IPV4_STR_SIZE], bc_str[IPV4_STR_SIZE];
    ipv4_to_string(gateway, gw_str);
    ipv4_to_string(mask, mask_str);
    ipv4_to_string(ipv4_network(gateway, mask), net_str);
    ipv4_to_string(ipv4_broadcast(gateway, mask), bc_str);

    printf("Gateway:   %s\n", gw_str);
    printf("Mask:      %s (/%d)\n", mask_str, ipv4_prefix_length(mask));
    printf("Network:   %s\n", net_str);
    printf("Broadcast: %s\n", bc_str);
    printf("Packets:   %llu\n", n);
    printf("Seed:      %u\n\n", seed);

    /* ---- имитация обработки пакетов ---- */
    unsigned long long own = 0, other = 0;

    for (unsigned long long i = 1; i <= n; i++) {
        uint32_t dest = random_address();
        int is_own = ipv4_same_subnet(dest, gateway, mask);

        if (is_own) own++;
        else        other++;

        if (verbose) {
            char dest_str[IPV4_STR_SIZE];
            ipv4_to_string(dest, dest_str);
            printf("#%llu: %-15s (0x%08X)  ->  %s\n", i, dest_str, (unsigned)dest,
                   is_own ? "own subnet: deliver directly"
                          : "other network: send via gateway");
        }
    }

    /* ---- статистика ---- */
    if (verbose) printf("\n");
    printf("Statistics:\n");
    printf("  Own subnet:      %llu packets (%.4f%%)\n", own, 100.0 * (double)own / (double)n);
    printf("  Other networks:  %llu packets (%.4f%%)\n", other, 100.0 * (double)other / (double)n);
    return 0;
}
