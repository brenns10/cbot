# Makefile for plugins.  Pretty darn simple.

CC=gcc
INC=-I../inc/
FLAGS=-fPIC
SOURCES=$(shell find . -type f -name "*.c")
OBJECTS=$(patsubst %.c,%.so,$(SOURCES))

CFG=release
ifeq ($(CFG),debug)
FLAGS += -g -DDEBUG
endif
ifneq ($(CFG),debug)
ifneq ($(CFG),release)
$(error Bad build configuration.  Choices are debug, release, coverage.)
endif
endif

all: $(OBJECTS)

help.so: help.c help.h

help.h: help.md help_translate.py
	./help_translate.py <help.md >help.h

%.so: %.c
	$(CC) $(FLAGS) $(INC) -shared $< -o $@

clean:
	rm -f $(OBJECTS)
	rm -f help.h
