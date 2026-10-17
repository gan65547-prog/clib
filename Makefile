# clib - C Middleware Library
# Top-level Makefile

CC      ?= gcc
AR      ?= ar
CFLAGS  := -Wall -Wextra -O2 -std=gnu11 -Wno-pointer-sign -Wno-sign-compare -Wno-unused-parameter -fPIC

# ── Platform detection ─────────────────────────────────────────────────

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

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

# ── OpenSSL ────────────────────────────────────────────────────────────

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

# ── FFmpeg ─────────────────────────────────────────────────────────────

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

# ── Rockchip MPP / RGA (aarch64 only) ─────────────────────────────────

MPP_CFLAGS := $(shell pkg-config --cflags rockchip_mpp 2>/dev/null)
MPP_LIBS   := $(shell pkg-config --libs rockchip_mpp 2>/dev/null)
ifeq ($(MPP_LIBS),)
  ifneq ($(wildcard /usr/lib/aarch64-linux-gnu/librockchip_mpp.so),)
    MPP_LIBS := -lrockchip_mpp
  else ifneq ($(wildcard /usr/lib/librockchip_mpp.so),)
    MPP_LIBS := -lrockchip_mpp
  else ifneq ($(wildcard /usr/lib/aarch64-linux-gnu/librockchip_mpp.a),)
    MPP_LIBS := -lrockchip_mpp
  endif
endif

RGA_CFLAGS := $(shell pkg-config --cflags rga 2>/dev/null)
RGA_LIBS   := $(shell pkg-config --libs rga 2>/dev/null)
ifeq ($(RGA_LIBS),)
  ifneq ($(wildcard /usr/lib/aarch64-linux-gnu/librga.so),)
    RGA_LIBS := -lrga
  else ifneq ($(wildcard /usr/lib/librga.so),)
    RGA_LIBS := -lrga
  endif
endif

# ── Source selection by platform ───────────────────────────────────────

# Cross-platform sources (always built)
CORE_SOURCES = \
    src/ml_base64.c \
    src/ml_bitmap.c \
    src/ml_container.c \
    src/ml_crypto.c \
    src/ml_cv_math.c \
    src/ml_dictionary.c \
    src/ml_event.c \
    src/ml_frame.c \
    src/ml_http.c \
    src/ml_iniparser.c \
    src/ml_json.c \
    src/ml_log.c \
    src/ml_mapkey.c \
    src/ml_md5.c \
    src/ml_mmap.c \
    src/ml_muxer.c \
    src/ml_pipeline_stage.c \
    src/ml_pkbuf.c \
    src/ml_pool.c \
    src/ml_queue.c \
    src/ml_rbtree.c \
    src/ml_ringbuf.c \
    src/ml_set_rbtree.c \
    src/ml_shm_pool.c \
    src/ml_stream.c \
    src/ml_string.c \
    src/ml_thread.c \
    src/ml_thread_pool.c \
    src/ml_timer.c \
    src/ml_websocket.c \
    src/ml_zero_copy.c \

# Platform-specific sources (one per module)

ifeq ($(UNAME_S),Darwin)
  # macOS: stubs for everything hardware-specific
  CAMERA_SRC = src/ml_camera.c src/ml_camera_stub.c
  CODEC_SRC  = src/ml_codec_stub.c
  RGA_SRC    = src/ml_rga_stub.c
else ifeq ($(UNAME_M),aarch64)
  # Linux aarch64 — probe for Rockchip MPP and RGA headers independently
  ifneq ($(wildcard /usr/include/rockchip/mpp_buffer.h),)
    CODEC_SRC = src/ml_codec_mpp.c
  else
    CODEC_SRC = src/ml_codec_stub.c
  endif
  ifneq ($(wildcard /usr/include/rga/RgaApi.h),)
    RGA_SRC = src/ml_rga.c
  else
    RGA_SRC = src/ml_rga_stub.c
  endif
  # V4L2 camera always on Linux; RTSP if FFmpeg is available
  CAMERA_SRC = src/ml_camera.c src/ml_camera_v4l2.c src/ml_camera_stub.c
  CFLAGS += -DML_CAMERA_HAS_V4L2
  ifneq ($(FFMPEG_LIBS),)
    CAMERA_SRC += src/ml_camera_rtsp.c
    CFLAGS += -DML_CAMERA_HAS_RTSP
  endif
else
  # Linux x86_64 (or other) — stubs for MPP/RGA, real V4L2 + optional RTSP
  CODEC_SRC  = src/ml_codec_stub.c
  RGA_SRC    = src/ml_rga_stub.c
  CAMERA_SRC = src/ml_camera.c src/ml_camera_v4l2.c src/ml_camera_stub.c
  CFLAGS += -DML_CAMERA_HAS_V4L2
  ifneq ($(FFMPEG_LIBS),)
    CAMERA_SRC += src/ml_camera_rtsp.c
    CFLAGS += -DML_CAMERA_HAS_RTSP
  endif
endif

LIB_SOURCES = $(CORE_SOURCES) $(CAMERA_SRC) $(CODEC_SRC) $(RGA_SRC)
LIB_OBJECTS = $(LIB_SOURCES:.c=.o)

# ── Shared library ─────────────────────────────────────────────────────

ifeq ($(UNAME_S),Darwin)
	SHARED_EXT  = dylib
	SHARED_FLAG = -dynamiclib -install_name @rpath/libclib.dylib
else
	SHARED_EXT  = so
	SHARED_FLAG = -shared -Wl,-soname,libclib.so
endif

STATIC_LIB  = libclib.a
SHARED_LIB  = libclib.$(SHARED_EXT)

.PHONY: all lib static shared test clean

all: static shared

lib: static shared

static: $(STATIC_LIB)

shared: $(SHARED_LIB)

$(STATIC_LIB): $(LIB_OBJECTS)
	$(AR) rcs $@ $(LIB_OBJECTS)

$(SHARED_LIB): $(LIB_OBJECTS)
	$(CC) $(SHARED_FLAG) -o $@ $(LIB_OBJECTS) -lpthread -lm $(OPENSSL_LIBS) $(FFMPEG_LIBS) $(MPP_LIBS) $(RGA_LIBS)

CFLAGS += -Iinclude $(OPENSSL_CFLAGS) $(FFMPEG_CFLAGS) $(MPP_CFLAGS) $(RGA_CFLAGS)

src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -c -o $@ $<

test:
	$(MAKE) -C tests test

clean:
	rm -f src/*.o $(STATIC_LIB) $(SHARED_LIB)
	$(MAKE) -C tests clean
	$(MAKE) -C template clean 2>/dev/null || true
