#-------------------------------------------------------------------------------
#
# File:         Makefile
#
# Author:       Stephen Brennan
#
# Date Created: Friday, 17 July 2015
#
# Description:  Generic C Makefile
#
# This is a generic makefile, suitable for any C programming project.  It comes
# with several features:
# - Running tests, with Valgrind.
# - Generation of documentation through Doxygen.  You'll need to provide a
#   Doxyfile.
# - Code coverage reports via gcov.
# - Build configurations: debug, release, and coverage.
# - Automatic dependency generation, so you never need to update this file.
#
# To use:
# 1. You should organize your project like this:
#    src/
#    |--- code.c
#    |--- module-1.h
#    |--- module-1/code.c
#    \--- module-2/code.c
#    test/
#    \--- test-code.c
#    inc/
#    \--- public-header.h
# 2. Fill out the variables labelled CONFIGURATION.
# 3. Build configurations are: debug, release, coverage.  Run make like this:
#    make CFG=configuration target
#    The default target is release, so you can omit it normally.
# 4. Targets:
#    - all: makes your main project
#    - test: makes and runs tests
#    - doc: builds documentation
#    - cov: generates code coverage (MUST have CFG=coverage)
#    - clean: removes object and binary files
#    - clean_{doc,cov,dep}: removes documentation/coverage/dependencies
#
# This code is in the public domain, for anyone to use or modify in any way.
#
#-------------------------------------------------------------------------------

# --- CONFIGURATION: Definitely change this stuff!
# PROJECT_NAME - not actually used.  but what's your project's name?
PROJECT_NAME=cbot
# PROJECT_TYPE - staticlib, dynamiclib, executable
PROJECT_TYPE=executable
# PROJECT_MAIN - filename within your source directory that contains main()
PROJECT_MAIN=main.c
# TARGET - the name you want your target to have (bin/release/[whatgoeshere])
TARGET=cbot
# TEST_TARGET - the name you want your tests to have (probably test)
TEST_TARGET=
# STATIC_LIBS - path to any static libs you need.  you may need to make a rule
# to generate them from subprojects.
STATIC_LIBS=libstephen/bin/release/libstephen.so
# EXTRA_INCLUDES - folders that should also be include directories (say, for
# static libs?)
EXTRA_INCLUDES=libstephen/inc

# --- DIRECTORY STRUCTURE: This structure is highly recommended, but you can
# change it.  The most important thing is that *none* of these directories are
# subdirectories of each other.  They should be completely disjoint.  Also,
# being too creative with directories could seriously mess up gcov, which is a
# finicky beast.
SOURCE_DIR=src
TEST_DIR=test
PLUGIN_DIR=plugin
INCLUDE_DIR=inc
OBJECT_DIR=obj
BINARY_DIR=bin
DEPENDENCY_DIR=dep
DOCUMENTATION_DIR=doc
COVERAGE_DIR=cov

# --- COMPILATION FLAGS: Things you may want/need to configure, but I've put
# them at sane defaults.
CC=gcc
FLAGS=-Wall -Wextra -Wno-unused-parameter
INC=-I$(INCLUDE_DIR) -I$(SOURCE_DIR) $(addprefix -I,$(EXTRA_INCLUDES))
CFLAGS=$(FLAGS) -std=c99 -fPIC $(INC) -c
LFLAGS=$(FLAGS)
PLUGIN_FLAGS=$(FLAGS) -std=c99 -fPIC $(INC) -shared
# Special libircclient related stuff.
LIBIRCCLIENT_VER=1.10
LIBIRCCLIENT_DIR=libircclient-$(LIBIRCCLIENT_VER)
LIBIRCCLIENT_TAR=$(LIBIRCCLIENT_DIR).tar.gz
ifneq ($(LIBIRCCLIENT_LOCAL),)
URL = http://downloads.sourceforge.net/project/libircclient/libircclient/$(LIBIRCCLIENT_VER)/$(LIBIRCCLIENT_TAR)
STATIC_LIBS += $(LIBIRCCLIENT_DIR)/src/libircclient.a
EXTRA_INCLUDES +=  $(LIBIRCCLIENT_DIR)/include
CFLAGS += -DLIBIRCCLIENT_LOCAL
LD=
else
LD=-lircclient
endif
LD += -lcrypto -lssl -ldl

# --- BUILD CONFIGURATIONS: Feel free to get creative with these if you'd like.
# The advantage here is that you can update variables (like compile flags) based
# on the build configuration.
CFG=release
ifeq ($(CFG),debug)
FLAGS += -g -DDEBUG
endif
ifeq ($(CFG),coverage)
CFLAGS += -fprofile-arcs -ftest-coverage
LFLAGS += -fprofile-arcs -lgcov
endif
ifneq ($(CFG),debug)
ifneq ($(CFG),release)
ifneq ($(CFG),coverage)
$(error Bad build configuration.  Choices are debug, release, coverage.)
endif
endif
endif

# --- FILENAME LISTS: (and other internal variables) You probably don't need to
# mess around with this stuff, unless you have a decent understanding of
# everything this Makefile does.
DIR_GUARD=@mkdir -p $(@D)
OBJECT_MAIN=$(OBJECT_DIR)/$(CFG)/$(SOURCE_DIR)/$(patsubst %.c,%.o,$(PROJECT_MAIN))

SOURCES=$(shell find $(SOURCE_DIR) -type f -name "*.c")
OBJECTS=$(patsubst $(SOURCE_DIR)/%.c,$(OBJECT_DIR)/$(CFG)/$(SOURCE_DIR)/%.o,$(SOURCES))

TEST_SOURCES="" #$(shell find $(TEST_DIR) -type f -name "*.c")
TEST_OBJECTS="" #$(patsubst $(TEST_DIR)/%.c,$(OBJECT_DIR)/$(CFG)/$(TEST_DIR)/%.o,$(TEST_SOURCES))

PLUGIN_SOURCES=$(shell find $(PLUGIN_DIR) -type f -name "*.c")
PLUGIN_OBJECTS=$(patsubst $(PLUGIN_DIR)/%.c,$(BINARY_DIR)/$(CFG)/$(PLUGIN_DIR)/%.so,$(PLUGIN_SOURCES))

DEPENDENCIES  = $(patsubst $(SOURCE_DIR)/%.c,$(DEPENDENCY_DIR)/$(SOURCE_DIR)/%.d,$(SOURCES))
DEPENDENCIES += $(patsubst $(TEST_DIR)/%.c,$(DEPENDENCY_DIR)/$(TEST_DIR)/%.d,$(TEST_SOURCES))
DEPENDENCIES += $(patsubst $(PLUGIN_DIR)/%.c,$(DEPENDENCY_DIR)/$(PLUGIN_DIR)/%.d,$(PLUGIN_SOURCES))

# --- GLOBAL TARGETS: You can probably adjust and augment these if you'd like.
.PHONY: all test clean clean_all clean_cov clean_doc plugin

all: $(BINARY_DIR)/$(CFG)/$(TARGET) $(PLUGIN_OBJECTS)
plugin: $(PLUGIN_OBJECTS)

test: $(BINARY_DIR)/$(CFG)/$(TEST_TARGET)
	valgrind $(BINARY_DIR)/$(CFG)/$(TEST_TARGET)

doc: $(SOURCES) $(TEST_SOURCES) Doxyfile
	doxygen

cov: $(BINARY_DIR)/$(CFG)/$(TEST_TARGET)
	@if [ "$(CFG)" != "coverage" ]; then \
	  echo "You must run 'make CFG=coverage coverage'."; \
	  exit 1; \
	fi
	rm -f coverage.info
	$(BINARY_DIR)/$(CFG)/$(TEST_TARGET)
	lcov -c -d $(OBJECT_DIR)/$(CFG) -b $(SOURCE_DIR) -o coverage.info
	lcov -e coverage.info "`pwd`/$(SOURCE_DIR)/*" -o coverage.info
	genhtml coverage.info -o $(COVERAGE_DIR)
	rm coverage.info

clean:
	rm -rf $(OBJECT_DIR)/$(CFG)/* $(BINARY_DIR)/$(CFG)/* $(SOURCE_DIR)/*.gch
	make -C libstephen clean
	rm -rf $(LIBIRCCLIENT_DIR)
	rm -f plugin/help.h

clean_all: clean_cov clean_doc
	rm -rf $(OBJECT_DIR) $(BINARY_DIR) $(DEPENDENCY_DIR) $(SOURCE_DIR)/*.gch
	rm -rf $(LIBIRCCLIENT_TAR)

clean_docs:
	rm -rf $(DOCUMENTATION_DIR)

clean_cov:
	rm -rf $(COVERAGE_DIR)

# STATIC LIBS: libstephen
libstephen/bin/release/libstephen.so:
	make PROJECT_TYPE=dynamiclib TARGET=libstephen.so -C libstephen

# STATIC LIBS: libircclient (if asked)
$(LIBIRCCLIENT_TAR):
	curl -L -o $(LIBIRCCLIENT_TAR) $(URL)

$(LIBIRCCLIENT_DIR)/config.status: $(LIBIRCCLIENT_TAR)
	tar xzf $(LIBIRCCLIENT_TAR)
	cd $(LIBIRCCLIENT_DIR) && ./configure --enable-openssl

$(LIBIRCCLIENT_DIR)/src/libircclient.a $(LIBIRCCLIENT_DIR)/include/libircclient.h: $(LIBIRCCLIENT_DIR)/config.status
	make -C $(LIBIRCCLIENT_DIR)

ifneq ($(LIBIRCCLIENT_LOCAL),)
$(SOURCES): $(LIBIRCCLIENT_DIR)/include/libircclient.h
endif

# RULE TO BUILD YOUR MAIN TARGET HERE: (you may have to edit this, but it it
# configurable).
$(BINARY_DIR)/$(CFG)/$(TARGET): $(OBJECTS) $(STATIC_LIBS)
	$(DIR_GUARD)
ifeq ($(PROJECT_TYPE),staticlib)
	ar rcs $@ $^
endif
ifeq ($(PROJECT_TYPE),dynamiclib)
	$(CC) $(LFLAGS) -shared  $^ -o $@ $(LD)
endif
ifeq ($(PROJECT_TYPE),executable)
	$(CC) $(LFLAGS) $^ -o $@ $(LD)
endif

# RULE TO BULID YOUR TEST TARGET HERE: (it's assumed that it's an executable)
$(BINARY_DIR)/$(CFG)/$(TEST_TARGET): $(filter-out $(OBJECT_MAIN),$(OBJECTS)) $(TEST_OBJECTS) $(STATIC_LIBS)
	$(DIR_GUARD)
	$(CC) $(LFLAGS) $^ -o $@

# --- Generic Compilation Command
$(OBJECT_DIR)/$(CFG)/%.o: %.c
	$(DIR_GUARD)
	$(CC) $(CFLAGS) $< -o $@

# --- Plugin compilation command.
$(BINARY_DIR)/$(CFG)/%.so: %.c
	$(DIR_GUARD)
	$(CC) $(PLUGIN_FLAGS) $< -o $@ libstephen/bin/release/libstephen.so

# --- Automatic Dependency Generation
$(DEPENDENCY_DIR)/%.d: %.c
	$(DIR_GUARD)
	$(CC) $(CFLAGS) -MM $< | sed -e 's!\(.*\)\.o:!$@ $(OBJECT_DIR)/$$(CFG)/$(<D)/\1.o:!' > $@

$(BINARY_DIR)/$(CFG)/$(PLUGIN_DIR)/help.so: plugin/help.h

plugin/help.h: plugin/help.md plugin/help_translate.py
	plugin/help_translate.py <plugin/help.md >plugin/help.h

# --- Include Generated Dependencies
ifneq "$(MAKECMDGOALS)" "clean_all"
-include $(DEPENDENCIES)
endif
