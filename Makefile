# clib - C Middleware Library
# Top-level Makefile

CC      ?= gcc
AR      ?= ar
CFLAGS  := -Wall -Wextra -O2 -std=gnu11 -Wno-pointer-sign -Wno-sign-compare -Wno-unused-parameter -fPIC

# Library sources (all .c files in src/)
LIB_SOURCES = $(filter-out %/main.c, $(wildcard src/*.c))
LIB_OBJECTS = $(LIB_SOURCES:.c=.o)

# mlfw framework — now maintained as sibling project at ../mlfw/
MLFW_DIR = ../mlfw

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	SHARED_EXT  = dylib
	SHARED_FLAG = -dynamiclib -install_name @rpath/libclib.dylib
else
	SHARED_EXT  = so
	SHARED_FLAG = -shared -Wl,-soname,libclib.so
endif

STATIC_LIB  = libclib.a
SHARED_LIB  = libclib.$(SHARED_EXT)

.PHONY: all lib static shared framework test clean

all: static shared

lib: static shared

static: $(STATIC_LIB)

shared: $(SHARED_LIB)

framework:
	@echo "mlfw framework is built separately — see ../mlfw/Makefile"

$(STATIC_LIB): $(LIB_OBJECTS)
	$(AR) rcs $@ $(LIB_OBJECTS)

$(SHARED_LIB): $(LIB_OBJECTS)
	$(CC) $(SHARED_FLAG) -o $@ $(LIB_OBJECTS) -lpthread -lm

CFLAGS += -Iinclude -Iinclude/mlfw

src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -c -o $@ $<

test:
	$(MAKE) -C tests test

clean:
	rm -f src/*.o src/mlfw/*.o $(STATIC_LIB) $(SHARED_LIB)
	$(MAKE) -C tests clean
	$(MAKE) -C template clean 2>/dev/null || true
