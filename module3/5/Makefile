CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS := -MMD -MP
# shm_open / sem_open: в старых версиях glibc нужны -lrt и -pthread
LDLIBS   := -lrt -pthread

COMMON   := common.o
PRODUCER := producer
CONSUMER := consumer
OBJS     := $(COMMON) producer.o consumer.o
DEPS     := $(OBJS:.o=.d)

all: $(PRODUCER) $(CONSUMER)

$(PRODUCER): producer.o $(COMMON)
	$(CC) $^ -o $@ $(LDLIBS)

$(CONSUMER): consumer.o $(COMMON)
	$(CC) $^ -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(DEPS) $(PRODUCER) $(CONSUMER)

-include $(DEPS)

.PHONY: all clean
