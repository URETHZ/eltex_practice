CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
TARGET  := pubsub
SRC     := main.c common.c broker.c publisher.c subscriber.c
OBJ     := $(SRC:.c=.o)

.PHONY: all clean test help

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

main.o:       main.c       common.h
common.o:     common.c     common.h
broker.o:     broker.c     common.h
publisher.o:  publisher.c  common.h
subscriber.o: subscriber.c common.h

test: $(TARGET)
	@chmod +x test_pubsub.sh
	@./test_pubsub.sh

clean:
	rm -f $(OBJ) $(TARGET)

help:
	@echo "make           — собрать $(TARGET)"
	@echo "make test      — запустить тесты"
	@echo "make clean     — удалить объектники и бинарник"
