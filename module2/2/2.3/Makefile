CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS := -MMD -MP
LDLIBS   := -lm

TARGET   := calc
SRCS     := main.c operations.c registry.c
OBJS     := $(SRCS:.c=.o)
DEPS     := $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: all run clean
