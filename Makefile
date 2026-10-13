CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=

TARGET := myls
SOURCES := src/main.c src/options.c src/list.c src/sort.c src/display.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

src/%.o: src/%.c include/ls.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

src/options.o: include/options.h
src/list.o: include/list.h include/display.h include/sort.h
src/sort.o: include/sort.h
src/display.o: include/display.h
src/main.o: include/options.h include/list.h

test: $(TARGET)
	./tests/smoke_test.sh

clean:
	rm -f $(OBJECTS) $(TARGET)
