# clib Core Library Test Suite
# Compiles on macOS and Linux

CC ?= gcc
CFLAGS = -Wall -Wextra -O0 -g -std=gnu11 -Wno-pointer-sign -Wno-sign-compare -Wno-unused-parameter
INCS = -I. -I../include -I../src -I../include/mlfw \
       -I../../mlfw/plugins/ipc_echo -I../../mlfw/plugins/ipc_ping \
       -I../../mlfw/plugins/device
LDLIBS = -lm -lpthread

SOURCES = main.c \
          test_rbtree.c \
          test_container.c \
          test_pool.c \
          test_string.c \
          test_bitmap.c \
          test_base64.c \
          test_integration.c \
          test_md5.c \
          test_pkbuf.c \
          test_mapkey.c \
          test_ringbuf.c \
          test_thread.c \
          test_log.c \
          test_event.c \
          test_timer.c \
          test_thread_pool.c \
          test_json.c \
          test_plugin.c \
          test_device_plugin.c \
          test_ipc.c \
          test_http.c \
          test_dictionary.c \
          test_iniparser.c \
          test_set_rbtree.c \
          test_shm_pool.c \
          test_zero_copy.c \
          ../src/ml_rbtree.c \
          ../src/ml_container.c \
          ../src/ml_mmap.c \
          ../src/ml_json.c \
          ../src/mlfw_http.c \
          ../src/ml_dictionary.c \
          ../src/ml_iniparser.c \
          ../src/ml_shm_pool.c \
          ../src/ml_zero_copy.c

OBJECTS = $(SOURCES:.c=.o)
TARGET = clib_test

%.o: %.c test_framework.h
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

PLUGIN_OBJS = ml_plugin_ipc_echo.o ml_plugin_ipc_ping.o ml_plugin_device.o

$(TARGET): $(OBJECTS) $(PLUGIN_OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(PLUGIN_OBJS) $(LDLIBS)

ml_plugin_ipc_echo.o: ../../mlfw/plugins/ipc_echo/ml_plugin_ipc_echo.c
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

ml_plugin_ipc_ping.o: ../../mlfw/plugins/ipc_ping/ml_plugin_ipc_ping.c
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

ml_plugin_device.o: ../../mlfw/plugins/device/ml_plugin_device.c
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

.PHONY: test clean all

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(PLUGIN_OBJS)

all: clean test
