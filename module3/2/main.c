#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int broker_main(int timeout_sec);
extern int publisher_main(const char *topic);
extern int subscriber_main(int topic_count, char *topics[]);

static void usage(const char *prog) {
    fprintf(stderr,
        "Использование:\n"
        "  %s -b [-t секунды]          — брокер (-t — таймаут, 0 = выкл)\n"
        "  %s -p <topic>               — издатель, публикует в <topic>\n"
        "  %s -s <topic> [topic ...]   — подписчик на список тем\n",
        prog, prog, prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(argv[0]); return 1; }

    if (strcmp(argv[1], "-b") == 0) {
        int timeout = 0;
        if (argc >= 4 && strcmp(argv[2], "-t") == 0)
            timeout = atoi(argv[3]);
        else if (argc != 2) { usage(argv[0]); return 1; }
        return broker_main(timeout);
    }
    if (strcmp(argv[1], "-p") == 0) {
        if (argc != 3) { usage(argv[0]); return 1; }
        return publisher_main(argv[2]);
    }
    if (strcmp(argv[1], "-s") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        return subscriber_main(argc - 2, &argv[2]);
    }

    usage(argv[0]);
    return 1;
}
