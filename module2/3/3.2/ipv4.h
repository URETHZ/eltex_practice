#ifndef IPV4_H
#define IPV4_H

#include <stdint.h>

#define IPV4_STR_SIZE 16   /* "255.255.255.255" + '\0' */

/* IPv4-адрес и маска хранятся как 32-битное число:
   192.168.1.10 -> 0xC0A8010A (старший байт - первый октет) */

typedef enum {
    IPV4_OK = 0,
    IPV4_ERR_FORMAT,        /* не четыре числа через точки */
    IPV4_ERR_OCTET_RANGE,   /* октет больше 255 */
    IPV4_ERR_MASK_PREFIX,   /* длина префикса вне 0..32 */
    IPV4_ERR_MASK_SHAPE     /* маска не из подряд идущих единиц, например 255.0.255.0 */
} Ipv4Status;

/* Разбор строки "a.b.c.d". Строго: ровно 4 десятичных октета 0..255 без
   лишних символов и без ведущих нулей ("01" не допускается). */
Ipv4Status ipv4_parse(const char *text, uint32_t *ip);

/* Разбор маски: "255.255.255.0", "24" или "/24". */
Ipv4Status ipv4_parse_mask(const char *text, uint32_t *mask);

/* Число единиц в маске (длина префикса). Маска должна быть корректной. */
int ipv4_prefix_length(uint32_t mask);

/* Число -> строка "a.b.c.d" */
void ipv4_to_string(uint32_t ip, char out[IPV4_STR_SIZE]);

uint32_t ipv4_network(uint32_t ip, uint32_t mask);      /* адрес сети */
uint32_t ipv4_broadcast(uint32_t ip, uint32_t mask);    /* широковещательный адрес */

/* 1, если адреса a и b принадлежат одной подсети с маской mask */
int ipv4_same_subnet(uint32_t a, uint32_t b, uint32_t mask);

const char *ipv4_status_message(Ipv4Status status);

#endif /* IPV4_H */
