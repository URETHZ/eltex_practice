#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ipv4.h"

Ipv4Status ipv4_parse(const char *text, uint32_t *ip)
{
    if (!text) return IPV4_ERR_FORMAT;

    uint32_t result = 0;
    int parts = 0;
    const char *p = text;

    for (;;) {
        if (!isdigit((unsigned char)*p)) return IPV4_ERR_FORMAT;

        const char *start = p;
        unsigned value = 0;
        while (isdigit((unsigned char)*p)) {
            if (value < 1000) value = value * 10 + (unsigned)(*p - '0');   /* без переполнения */
            p++;
        }
        size_t len = (size_t)(p - start);

        if (len > 1 && *start == '0') return IPV4_ERR_FORMAT;   /* ведущие нули */
        if (len > 3 || value > 255) return IPV4_ERR_OCTET_RANGE;

        result = (result << 8) | value;
        parts++;

        if (*p == '.') {
            if (parts == 4) return IPV4_ERR_FORMAT;            /* пятая часть */
            p++;
            continue;
        }
        break;
    }

    if (*p != '\0' || parts != 4) return IPV4_ERR_FORMAT;
    *ip = result;
    return IPV4_OK;
}

/* Маска корректна, если это подряд идущие единицы, затем нули */
static int mask_is_contiguous(uint32_t mask)
{
    uint32_t inverted = ~mask;
    return (inverted & (inverted + 1u)) == 0;
}

Ipv4Status ipv4_parse_mask(const char *text, uint32_t *mask)
{
    if (!text || text[0] == '\0') return IPV4_ERR_FORMAT;

    if (strchr(text, '.')) {                       /* 255.255.255.0 */
        uint32_t value;
        Ipv4Status st = ipv4_parse(text, &value);
        if (st != IPV4_OK) return st;
        if (!mask_is_contiguous(value)) return IPV4_ERR_MASK_SHAPE;
        *mask = value;
        return IPV4_OK;
    }

    const char *p = text;                          /* 24 или /24 */
    if (*p == '/') p++;
    if (!isdigit((unsigned char)*p)) return IPV4_ERR_FORMAT;

    unsigned prefix = 0;
    size_t digits = 0;
    for (; isdigit((unsigned char)*p); p++, digits++) {
        if (prefix < 1000) prefix = prefix * 10 + (unsigned)(*p - '0');
    }
    if (*p != '\0') return IPV4_ERR_FORMAT;
    if (digits > 2 || prefix > 32) return IPV4_ERR_MASK_PREFIX;

    *mask = (prefix == 0) ? 0u : (0xFFFFFFFFu << (32 - prefix));   /* сдвиг на 32 запрещён */
    return IPV4_OK;
}

int ipv4_prefix_length(uint32_t mask)
{
    int n = 0;
    for (; mask & 0x80000000u; mask <<= 1) n++;
    return n;
}

void ipv4_to_string(uint32_t ip, char out[IPV4_STR_SIZE])
{
    snprintf(out, IPV4_STR_SIZE, "%u.%u.%u.%u",
             (unsigned)((ip >> 24) & 0xFF), (unsigned)((ip >> 16) & 0xFF),
             (unsigned)((ip >> 8) & 0xFF), (unsigned)(ip & 0xFF));
}

uint32_t ipv4_network(uint32_t ip, uint32_t mask)
{
    return ip & mask;
}

uint32_t ipv4_broadcast(uint32_t ip, uint32_t mask)
{
    return ip | ~mask;
}

int ipv4_same_subnet(uint32_t a, uint32_t b, uint32_t mask)
{
    return (a & mask) == (b & mask);
}

const char *ipv4_status_message(Ipv4Status status)
{
    switch (status) {
    case IPV4_OK:              return "OK";
    case IPV4_ERR_FORMAT:      return "invalid format, expected four numbers separated by dots (a.b.c.d)";
    case IPV4_ERR_OCTET_RANGE: return "each number must be in the range 0..255";
    case IPV4_ERR_MASK_PREFIX: return "prefix length must be in the range 0..32";
    case IPV4_ERR_MASK_SHAPE:  return "mask must be a block of ones followed by zeros (e.g. 255.255.255.0)";
    }
    return "unknown error";
}
