# clib examples — standalone build
# Each example links against libclib in the parent directory.

CC      ?= gcc
CFLAGS  := -Wall -Wextra -O2 -std=gnu11 -I../include
CLIB_DIR := $(abspath ..)
LDFLAGS  := -L$(CLIB_DIR) -lclib -lpthread -lm -Wl,-rpath,$(CLIB_DIR)

EXAMPLES := pool ringbuf thread_pool json

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

pool: pool.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

ringbuf: ringbuf.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

thread_pool: thread_pool.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

json: json.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(EXAMPLES)
