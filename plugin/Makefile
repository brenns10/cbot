# Makefile for plugins.  Pretty darn simple.

CC=gcc
INC=-I../inc/
FLAGS=-fPIC
SOURCES=$(shell find . -type f -name "*.c")
OBJECTS=$(patsubst %.c,%.so,$(SOURCES))

all: $(OBJECTS)

%.so: %.c
	$(CC) $(FLAGS) $(INC) -shared $< -o $@
