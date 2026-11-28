CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =

TARGETS = server client

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)
	rm -rf received

.PHONY: all clean