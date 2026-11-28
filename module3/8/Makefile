CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =

TARGET = udp_sniffer

all: $(TARGET)

$(TARGET): udp_sniffer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET) capture.txt

.PHONY: all clean