# Copyright (c) 2026 Gan
# SPDX-License-Identifier: MIT

# mlfw application template Makefile
#
# Usage:
#   1. Build clib first:  cd .. && make
#   2. Build your app:    make
#   3. Run:               ./myapp -w /opt/myapp -m /tmp/myapp
#
# Your app only needs to provide app.c with g_ml_cores[] and g_ml_modules[].

CLIB_DIR  = ..
INCLUDES  = -I$(CLIB_DIR)/include -I$(CLIB_DIR)/include/mlfw
CFLAGS   += -Wall -Wextra -O2 -std=gnu11 $(INCLUDES)
LDFLAGS  += -L$(CLIB_DIR) -lclib -lpthread -lm

# Framework main.o (provides main())
MAIN_O    = $(CLIB_DIR)/src/mlfw/main.o

APP       = myapp
SRCS      = app.c
OBJS      = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(APP)

$(APP): $(OBJS) $(MAIN_O)
	$(CC) -o $@ $(OBJS) $(MAIN_O) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(APP)
	./$(APP) -w /opt/myapp -m /tmp/myapp

clean:
	rm -f $(APP) $(OBJS)
