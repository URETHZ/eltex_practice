# Makefile для UDP-чата (Linux)
# Сборка:   make
# Запуск:   make run NICK=Alice
# Очистка:  make clean

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2 -std=gnu11
LDFLAGS  ?=
LDLIBS   := -pthread

TARGET   := udp_chat
SRC      := udp_chat.c
OBJ      := $(SRC:.c=.o)

# Имя пользователя для `make run` (можно переопределить: make run NICK=Bob)
NICK     ?= user

.PHONY: all run clean rebuild

all: $(TARGET)

# Линковка
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Компиляция .c -> .o с автоматической генерацией зависимостей от заголовков
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# Удобная цель для быстрого запуска
run: $(TARGET)
	./$(TARGET) $(NICK)

# Полная пересборка
rebuild: clean all

clean:
	rm -f $(TARGET) $(OBJ) $(OBJ:.o=.d)

# Подключаем сгенерированные файлы зависимостей (.d), если они есть
-include $(OBJ:.o=.d)