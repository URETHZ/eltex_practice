#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>

#define SNAP_LEN   65535
#define OUT_FILE   "capture.txt"

static int       raw_fd = -1;
static FILE     *out_fp = NULL;
static struct timespec t_start;
static volatile sig_atomic_t running = 1;

/* ---------- типы фильтров ---------- */
enum filter_t {
    FILTER_NONE = 0,
    FILTER_CHAT,          /* задание 6: порт 9090 + префиксы JOIN/LEAVE/CHAT */
    FILTER_DNS            /* порт 53 */
};

/* ---------- утилиты ---------- */

static void mac_str(const unsigned char *m, char *out) {
    snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             m[0], m[1], m[2], m[3], m[4], m[5]);
}

static double elapsed_sec(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - t_start.tv_sec)
         + (now.tv_nsec - t_start.tv_nsec) / 1e9;
}

/* ---------- DNS: разбор первого вопроса (QNAME) ---------- */

static int dns_extract_qname(const unsigned char *payload, int payload_len,
                             char *out, size_t out_sz)
{
    if (payload_len < 12) return -1;           /* слишком короткий DNS-заголовок */
    int qdcount = (payload[4] << 8) | payload[5];
    if (qdcount < 1) return -1;

    int off = 12;                              /* после DNS-заголовка */
    size_t pos = 0;
    int first_label = 1;

    while (off < payload_len) {
        int len = payload[off++];
        if (len == 0) break;                   /* конец QNAME */
        if ((len & 0xC0) == 0xC0) break;       /* compression — не разбираем */
        if (off + len > payload_len) break;

        if (!first_label && pos + 1 < out_sz) out[pos++] = '.';
        first_label = 0;

        for (int i = 0; i < len && pos + 1 < out_sz; i++)
            out[pos++] = (char)payload[off + i];
        off += len;
    }
    out[pos] = '\0';
    return 0;
}

/* ---------- фильтр чата (задание 6) ---------- */

static int match_chat(const unsigned char *payload, int payload_len) {
    if (payload_len < 4) return 0;
    /* префиксы: "JOIN|", "LEAVE|", "CHAT|" */
    if (payload_len >= 5  && memcmp(payload, "JOIN|",  5) == 0) return 1;
    if (payload_len >= 6  && memcmp(payload, "LEAVE|", 6) == 0) return 1;
    if (payload_len >= 5  && memcmp(payload, "CHAT|",  5) == 0) return 1;
    return 0;
}

/* ---------- вывод одного пакета ---------- */

static void print_packet(double t,
                         const char *mac_src, const char *mac_dst,
                         const char *ip_src,  const char *ip_dst,
                         uint16_t sport, uint16_t dport,
                         int payload_len,
                         const unsigned char *payload,
                         enum filter_t filter)
{
    char line[4096];
    int n = 0;

    n += snprintf(line + n, sizeof(line) - n,
                  "[+%09.6f] MAC %s -> %s | IP %s:%u -> %s:%u | UDP len=%d",
                  t, mac_src, mac_dst,
                  ip_src, sport, ip_dst, dport, payload_len);

    if (filter == FILTER_CHAT && payload_len > 0) {
        /* payload — человекочитаемый текст */
        char safe[512];
        int L = payload_len < (int)sizeof(safe) - 1
                ? payload_len : (int)sizeof(safe) - 1;
        memcpy(safe, payload, L);
        safe[L] = '\0';
        /* убираем переводы строк, чтобы не ломать вывод */
        for (int i = 0; i < L; i++)
            if (safe[i] == '\n' || safe[i] == '\r') safe[i] = ' ';
        n += snprintf(line + n, sizeof(line) - n, " | %s", safe);
    } else if (filter == FILTER_DNS && payload_len >= 12) {
        int qr = (payload[2] >> 7) & 1;
        char qname[256] = {0};
        dns_extract_qname(payload, payload_len, qname, sizeof(qname));
        n += snprintf(line + n, sizeof(line) - n,
                      " | DNS %s %s",
                      qr ? "RESPONSE" : "QUERY",
                      qname[0] ? qname : "(no qname)");
    }

    n += snprintf(line + n, sizeof(line) - n, "\n");

    fputs(line, stdout);
    if (out_fp) fputs(line, out_fp);
    fflush(stdout);
    if (out_fp) fflush(out_fp);
}

/* ---------- основной цикл захвата ---------- */

static void capture_loop(enum filter_t filter) {
    unsigned char buf[SNAP_LEN];
    struct sockaddr_ll sll;
    socklen_t sll_len = sizeof(sll);

    while (running) {
        ssize_t n = recvfrom(raw_fd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&sll, &sll_len);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recvfrom");
            break;
        }
        if (n < (ssize_t)(sizeof(struct ether_header)
                          + sizeof(struct iphdr)
                          + sizeof(struct udphdr)))
            continue;  /* не Ethernet+IP+UDP */

        /* Ethernet */
        struct ether_header *eth = (struct ether_header *)buf;
        if (ntohs(eth->ether_type) != ETHERTYPE_IP) continue;

        /* IP */
        struct iphdr *ip = (struct iphdr *)(buf + sizeof(struct ether_header));
        if (ip->protocol != IPPROTO_UDP) continue;
        int ip_hlen = ip->ihl * 4;
        if (ip_hlen < 20) continue;

        /* UDP */
        unsigned char *udp_start = (unsigned char *)ip + ip_hlen;
        if ((size_t)n < (sizeof(struct ether_header) + ip_hlen + sizeof(struct udphdr)))
            continue;
        struct udphdr *udp = (struct udphdr *)udp_start;

        uint16_t sport = ntohs(udp->source);
        uint16_t dport = ntohs(udp->dest);
        uint16_t udp_len = ntohs(udp->len);
        int payload_len = udp_len - sizeof(struct udphdr);
        if (payload_len < 0) continue;
        unsigned char *payload = udp_start + sizeof(struct udphdr);

        /* Применяем фильтр */
        int accept = 0;
        if (filter == FILTER_CHAT) {
            if (sport == 9090 || dport == 9090)
                accept = match_chat(payload, payload_len);
        } else if (filter == FILTER_DNS) {
            if (sport == 53 || dport == 53)
                accept = 1;
        }
        if (!accept) continue;

        /* Формируем читаемые строки */
        char mac_src[18], mac_dst[18];
        mac_str(eth->ether_shost, mac_src);
        mac_str(eth->ether_dhost, mac_dst);

        char ip_src[INET_ADDRSTRLEN], ip_dst[INET_ADDRSTRLEN];
        struct in_addr sa = { .s_addr = ip->saddr };
        struct in_addr da = { .s_addr = ip->daddr };
        inet_ntop(AF_INET, &sa, ip_src, sizeof(ip_src));
        inet_ntop(AF_INET, &da, ip_dst, sizeof(ip_dst));

        print_packet(elapsed_sec(),
                     mac_src, mac_dst,
                     ip_src, ip_dst,
                     sport, dport,
                     payload_len, payload, filter);
    }
}

/* ---------- обработка сигналов ---------- */

static void on_signal(int sig) {
    (void)sig;
    running = 0;
}

/* ---------- меню ---------- */

static enum filter_t choose_filter(void) {
    printf("=== UDP RAW sniffer ===\n");
    printf("Select filter:\n");
    printf("  1) Chat from task 6 (UDP port 9090, JOIN/LEAVE/CHAT)\n");
    printf("  2) DNS (UDP port 53)\n");
    printf("Choice: ");
    fflush(stdout);
    int c = fgetc(stdin);
    while (fgetc(stdin) != '\n') {}   /* очистка буфера */
    if (c == '1') return FILTER_CHAT;
    if (c == '2') return FILTER_DNS;
    return FILTER_NONE;
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    const char *iface = "eth0";
    if (argc >= 2) iface = argv[1];

    enum filter_t filter = choose_filter();
    if (filter == FILTER_NONE) {
        fprintf(stderr, "Unknown filter, exit.\n");
        return 1;
    }

    /* RAW-сокет уровня Ethernet */
    raw_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_fd < 0) {
        perror("socket(AF_PACKET) — run as root / sudo");
        return 1;
    }

    /* Опционально: привязка к конкретному интерфейсу */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(raw_fd, SIOCGIFINDEX, &ifr) == 0) {
        struct sockaddr_ll sll;
        memset(&sll, 0, sizeof(sll));
        sll.sll_family   = AF_PACKET;
        sll.sll_ifindex  = ifr.ifr_ifindex;
        sll.sll_protocol = htons(ETH_P_ALL);
        if (bind(raw_fd, (struct sockaddr *)&sll, sizeof(sll)) < 0)
            perror("bind to interface (non-fatal)");
    }

    /* Приёмный буфер побольше, чтобы не терять пакеты */
    int rcvbuf = 4 * 1024 * 1024;
    setsockopt(raw_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    out_fp = fopen(OUT_FILE, "w");
    if (!out_fp) perror("fopen capture file (non-fatal)");

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    printf("[*] Capturing on iface '%s' (filter=%s). Press Ctrl+C to stop.\n",
           iface,
           filter == FILTER_CHAT ? "chat-task6" : "dns");
    printf("[*] Output file: %s\n", OUT_FILE);

    capture_loop(filter);

    printf("\n[*] Capture stopped. Total time: %.3f sec\n", elapsed_sec());
    if (out_fp) {
        fclose(out_fp);
        printf("[*] Saved to %s\n", OUT_FILE);
    }
    close(raw_fd);
    return 0;
}