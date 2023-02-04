#!/usr/bin/make
.SUFFIXES:
.PHONY: all run pack clean
.SILENT: run

SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.o)
TAR = memory

CFLAGS = -std=gnu11 -c -g -Os -Wall -MMD -MP

DEP = $(OBJ:%.o=%.d)
-include $(DEP)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

$(TAR): $(OBJ)
	$(CC) -o $@ $^

all: $(TAR)

run: all
	./$(TAR)

clean:
	$(RM) $(RMFILES) $(TAR) $(OBJ) $(DEP)
