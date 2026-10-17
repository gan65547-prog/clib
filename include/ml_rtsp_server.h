/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_rtsp_server — RTSP video streaming via mediamtx + ffmpeg pipe.
 *
 * Starts mediamtx (RTSP server) and ffmpeg (encoder + publisher) as
 * managed child processes.  The caller feeds raw video frames through
 * a pipe; ffmpeg encodes and publishes them to mediamtx, which serves
 * the RTSP stream to clients (VLC / ffplay / etc.).
 *
 * This is a pipeline side-channel, not part of the inference hot path.
 * feed() is non-blocking and failures are silent — the RTSP stream can
 * drop without affecting the main pipeline.
 *
 * Lifecycle:
 *   create → feed* → destroy
 *
 * Dependencies:
 *   - mediamtx binary (RTSP server, https://github.com/bluenviron/mediamtx)
 *   - ffmpeg binary (encoding + RTSP publishing)
 *
 * Cross-platform:
 *   - Linux (RK3588):    h264_rkmpp or libx264
 *   - macOS (dev/test):  libx264 via Homebrew ffmpeg
 *   - No mediamtx/ffmpeg: create() returns NULL, module compiles as no-op stub
 */

#ifndef __ML_RTSP_SERVER_H__
#define __ML_RTSP_SERVER_H__

#include <stddef.h>
#include <stdint.h>

#include "ml_code.h"

/* ── forward declarations ─────────────────────────────────────────────── */

typedef struct ml_rtsp_server_s ml_rtsp_server_t;
typedef struct ml_pool_s       ml_pool_t;

/* ── pixel format ─────────────────────────────────────────────────────── */

typedef enum {
    ML_RTSP_PIX_NV12  = 0,  /* YUV 4:2:0 semi-planar (default from MPP/camera) */
    ML_RTSP_PIX_BGR24 = 1,  /* OpenCV BGR (macOS file-source testing)          */
    ML_RTSP_PIX_YUV420P = 2, /* planar YUV 4:2:0                                */
} ml_rtsp_pix_fmt_t;

/* ── configuration ────────────────────────────────────────────────────── */

typedef struct {
    /* ── video ─────────────────────────────────────────── */
    int              width;
    int              height;
    int              fps_num;         /* numerator, e.g. 30      */
    int              fps_den;         /* denominator, e.g. 1     */
    ml_rtsp_pix_fmt_t pix_fmt;

    /* ── stream identity ────────────────────────────────── */
    const char      *stream_path;     /* URL path, e.g. "stream"
                                         → rtsp://<host>:8554/stream */
    int              port;            /* mediamtx RTSP port (default 8554) */

    /* ── binary paths ──────────────────────────────────── */
    const char      *mediamtx_path;   /* NULL → search $PATH           */
    const char      *ffmpeg_path;     /* NULL → search $PATH           */

    /* ── encoding ──────────────────────────────────────── */
    const char      *ffmpeg_vcodec;   /* NULL → "libx264"             */
    const char      *ffmpeg_preset;   /* NULL → "ultrafast"           */
    const char      *ffmpeg_tune;     /* NULL → "zerolatency"         */
    int              ffmpeg_bitrate;  /* bps, 0 → ffmpeg default       */
} ml_rtsp_server_cfg_t;

/* defaults */
#define ML_RTSP_SERVER_CFG_DEFAULT {        \
    1920, 1080,                              \
    30, 1,                                   \
    ML_RTSP_PIX_NV12,                        \
    "stream",                                \
    8554,                                    \
    NULL, NULL,                              \
    NULL, NULL,                              \
    NULL,                                    \
    0                                        \
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

/* Create RTSP server, start mediamtx + ffmpeg subprocesses.
 * cfg: configuration (NULL → defaults).
 * pool: optional memory pool (NULL → malloc/free).
 * Returns NULL if mediamtx or ffmpeg cannot be started. */
ml_rtsp_server_t *ml_rtsp_server_create(const ml_rtsp_server_cfg_t *cfg,
                                         ml_pool_t *pool);

/* Stop subprocesses, close pipes, free resources. Safe on NULL. */
void ml_rtsp_server_destroy(ml_rtsp_server_t *srv);

/* ── frame feeding ────────────────────────────────────────────────────── */

/* Feed one raw video frame to the encoding pipeline.
 * data: raw pixel data (format as configured).
 * len:  size in bytes (width * height * bytes_per_pixel).
 * Returns 0 on success, -1 if the pipe is broken (RTSP process died).
 * Non-blocking on macOS; may block briefly on Linux if pipe buffer is full. */
int ml_rtsp_server_feed(ml_rtsp_server_t *srv,
                         const uint8_t *data, size_t len);

/* ── query ────────────────────────────────────────────────────────────── */

/* Full RTSP URL, e.g. "rtsp://192.168.1.100:8554/stream".
 * Returns NULL if server is not running.  String is valid until destroy(). */
const char *ml_rtsp_server_url(ml_rtsp_server_t *srv);

/* Return 1 if subprocesses are alive and pipe is writable. */
int ml_rtsp_server_running(ml_rtsp_server_t *srv);

/* Last error message. Returns NULL if no error. */
const char *ml_rtsp_server_error(ml_rtsp_server_t *srv);

#endif /* __ML_RTSP_SERVER_H__ */
