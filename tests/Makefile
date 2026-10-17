# clib Core Library Test Suite
# Compiles on macOS and Linux

CC ?= gcc
CFLAGS = -Wall -Wextra -O0 -g -std=gnu11 -Wno-pointer-sign -Wno-sign-compare -Wno-unused-parameter $(OPENSSL_CFLAGS) $(FFMPEG_CFLAGS)
INCS = -I. -I../include -I../src -I../include/mlfw \
       -I../../mlfw/plugins/ipc_echo -I../../mlfw/plugins/ipc_ping \
       -I../../mlfw/plugins/device
LDLIBS = -lm -lpthread

# ── Path helpers for conda/miniforge ───────────────────────────────────

CONDA_PREFIX ?= $(shell echo $$CONDA_PREFIX)
ifneq ($(CONDA_PREFIX),)
  CONDA_FOUND := 1
else ifneq ($(wildcard $(HOME)/miniforge3),)
  CONDA_PREFIX := $(HOME)/miniforge3
  CONDA_FOUND := 1
else ifneq ($(wildcard /opt/conda),)
  CONDA_PREFIX := /opt/conda
  CONDA_FOUND := 1
endif

UNAME_S := $(shell uname -s)

# FFmpeg (for test_mux)
FFMPEG_CFLAGS := $(shell pkg-config --cflags libavformat libavcodec libavutil libswscale 2>/dev/null)
FFMPEG_LIBS   := $(shell pkg-config --libs libavformat libavcodec libavutil libswscale 2>/dev/null)
ifeq ($(FFMPEG_LIBS),)
  ifeq ($(UNAME_S),Darwin)
    FFMPEG_CFLAGS := -I/opt/homebrew/opt/ffmpeg/include
    FFMPEG_LIBS   := -L/opt/homebrew/opt/ffmpeg/lib -lavformat -lavcodec -lavutil -lswscale
  else ifeq ($(CONDA_FOUND),1)
    FFMPEG_CFLAGS := -I$(CONDA_PREFIX)/include
    FFMPEG_LIBS   := -L$(CONDA_PREFIX)/lib -lavformat -lavcodec -lavutil -lswscale
  endif
endif

# OpenSSL (for test_crypto)
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS   := $(shell pkg-config --libs openssl 2>/dev/null)
ifeq ($(OPENSSL_LIBS),)
  ifeq ($(UNAME_S),Darwin)
    OPENSSL_CFLAGS := -I/opt/homebrew/opt/openssl@3/include
    OPENSSL_LIBS   := -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto
  else ifeq ($(CONDA_FOUND),1)
    OPENSSL_CFLAGS := -I$(CONDA_PREFIX)/include
    OPENSSL_LIBS   := -L$(CONDA_PREFIX)/lib -lssl -lcrypto
  else
    OPENSSL_LIBS   := -lssl -lcrypto
  endif
endif

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
          test_dictionary.c \
          test_iniparser.c \
          test_set_rbtree.c \
          test_shm_pool.c \
          test_zero_copy.c \
          test_crypto.c \
          test_queue.c \
          test_http_parser.c \
          test_websocket.c \
          test_mux.c \
          test_codec.c \
          test_camera.c \
          test_rga.c \
          test_cv_math.c \
          test_pipeline_stage.c \
          ../src/ml_rbtree.c \
          ../src/ml_container.c \
          ../src/ml_mmap.c \
          ../src/ml_json.c \
          ../src/ml_dictionary.c \
          ../src/ml_iniparser.c \
          ../src/ml_shm_pool.c \
          ../src/ml_zero_copy.c \
          ../src/ml_cv_math.c \
          ../src/ml_pipeline_stage.c

OBJECTS = $(SOURCES:.c=.o)
TARGET = clib_test

%.o: %.c test_framework.h
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDLIBS) $(OPENSSL_LIBS) $(FFMPEG_LIBS)

.PHONY: test clean all

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(PLUGIN_OBJS)

all: clean test
