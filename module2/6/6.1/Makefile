CC       := gcc
AR       := ar
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS := -MMD -MP -Ilib

# Статическая библиотека: libdlist.a собирается из lib/dlist.c
LIB      := lib/libdlist.a
LIB_OBJS := lib/dlist.o

# Программа: исходники приложения + библиотека
TARGET   := phonebook
APP_OBJS := src/main.o src/contact.o

TEST     := tests/test_dlist
DEPS     := $(LIB_OBJS:.o=.d) $(APP_OBJS:.o=.d)

all: $(TARGET)

# Архив из объектных файлов: ar r - добавить, c - создать, s - построить индекс
$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

# Объектные файлы приложения идут ПЕРЕД библиотекой: линковщик берёт из .a только то, что нужно
$(TARGET): $(APP_OBJS) $(LIB)
	$(CC) $(APP_OBJS) -Llib -ldlist -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

lib: $(LIB)

# Проверка библиотеки отдельно от телефонной книги
test: $(LIB) tests/test_dlist.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_dlist.c -Llib -ldlist -o $(TEST)
	./$(TEST)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(LIB_OBJS) $(APP_OBJS) $(DEPS) $(LIB) $(TARGET) $(TEST)

-include $(DEPS)

.PHONY: all lib test run clean
