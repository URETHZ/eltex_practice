# Проверка структуры папок: Makefile должен лежать рядом с папками src и lib
REQUIRED := src/main.c src/contact.c src/contact.h lib/dlist.c lib/dlist.h
$(foreach f,$(REQUIRED),$(if $(wildcard $(f)),,$(error Missing file '$(f)'. Keep the folder layout: Makefile, src/, lib/, tests/ and run make in the folder with the Makefile)))

CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS := -MMD -MP -Ilib

# Динамическая (разделяемая) библиотека: libdlist.so собирается из lib/dlist.c
LIB      := lib/libdlist.so
LIB_OBJS := lib/dlist.o

# Программа: исходники приложения + динамическая библиотека
TARGET   := phonebook
APP_OBJS := src/main.o src/contact.o

TEST     := tests/test_dlist
DEPS     := $(LIB_OBJS:.o=.d) $(APP_OBJS:.o=.d)

all: $(TARGET)

# Код библиотеки должен быть позиционно-независимым (-fPIC): система загружает
# её по любому адресу и делит между программами. Для кода приложения это не нужно.
lib/%.o: lib/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# -shared: собрать разделяемую библиотеку из объектных файлов
$(LIB): $(LIB_OBJS)
	$(CC) -shared $^ -o $@

# -Llib -ldlist: при линковке найти lib/libdlist.so (в программу копируется
#   только ссылка на библиотеку, а не её код).
# -Wl,-rpath,'$$ORIGIN/lib': запомнить в программе путь поиска библиотеки при запуске -
#   папка lib рядом с исполняемым файлом. Без этого пришлось бы задавать LD_LIBRARY_PATH.
$(TARGET): $(APP_OBJS) $(LIB)
	$(CC) $(APP_OBJS) -Llib -ldlist -Wl,-rpath,'$$ORIGIN/lib' -o $@

lib: $(LIB)

# Проверка библиотеки отдельно от телефонной книги
test: $(LIB) tests/test_dlist.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_dlist.c -Llib -ldlist -Wl,-rpath,'$$ORIGIN/../lib' -o $(TEST)
	./$(TEST)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(LIB_OBJS) $(APP_OBJS) $(DEPS) $(LIB) $(TARGET) $(TEST)

-include $(DEPS)

.PHONY: all lib test run clean
