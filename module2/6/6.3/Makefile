CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS := -MMD -MP -Iinclude -Isrc

# ---- Настройки под систему ----
ifeq ($(OS),Windows_NT)
	EXE            := .exe
	PLUGIN_EXT     := .dll
	PLUGIN_CFLAGS  :=
	PLUGIN_LDFLAGS := -shared -static-libgcc
	APP_LIBS       :=
else
	EXE            :=
	PLUGIN_CFLAGS  := -fPIC
	UNAME_S        := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
	    PLUGIN_EXT     := .dylib
	    PLUGIN_LDFLAGS := -dynamiclib
	    APP_LIBS       :=
	else
	    PLUGIN_EXT     := .so
	    PLUGIN_LDFLAGS := -shared
	    APP_LIBS       := -ldl
	endif
endif

# ---- Программа ----
TARGET   := calc$(EXE)
APP_OBJS := src/main.o src/loader.o src/registry.o

# ---- Библиотеки: одна функция - одна библиотека ----
PLUGIN_NAMES := add sub mul div pow sqrt fact mod sum mean max
PLUGINS      := $(addprefix plugins/,$(addsuffix $(PLUGIN_EXT),$(PLUGIN_NAMES)))
EXTRA_PLUGIN := plugins/cube$(PLUGIN_EXT)

DEPS := $(APP_OBJS:.o=.d)

all: $(TARGET) plugins

$(TARGET): $(APP_OBJS)
	$(CC) $(APP_OBJS) -o $@ $(APP_LIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Каждая библиотека собирается из своего файла plugins_src/<имя>.c
plugins/%$(PLUGIN_EXT): plugins_src/%.c include/plugin_api.h
	$(CC) -Iinclude $(CFLAGS) $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $< -o $@ -lm

plugins: $(PLUGINS)

# Дополнительная библиотека (cube) - для проверки добавления функции "на лету":
# запустите программу, соберите её командой make extra и выберите "Refresh menu".
extra: $(EXTRA_PLUGIN)

run: all
	./$(TARGET)

clean:
	rm -f $(APP_OBJS) $(DEPS) $(TARGET) $(PLUGINS) $(EXTRA_PLUGIN)

-include $(DEPS)

.PHONY: all plugins extra run clean
