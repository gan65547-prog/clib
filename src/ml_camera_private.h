/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_camera_private.h — Internal declarations for camera backends.
 *
 * Each backend (v4l2, rtsp, stub) implements the functions below with
 * a backend-specific prefix.  The dispatcher (ml_camera.c) provides
 * the public ml_camera_* API and delegates to these internally.
 */

#ifndef __ML_CAMERA_PRIVATE_H__
#define __ML_CAMERA_PRIVATE_H__

#include "ml_camera.h"
#include "ml_frame.h"
#include "ml_pool.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── unified internal struct ──────────────────────────────────────────── */

#if defined(ML_CAMERA_HAS_RTSP)
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#endif

#if defined(ML_CAMERA_HAS_V4L2)
typedef struct {
    void   *start;
    size_t  length;
    int     fd;
    int     queued;
} ml_camera_v4l2_buf_t;
#endif

struct ml_camera_s {
    int              backend;
    ml_camera_cfg_t  cfg;
    ml_pool_t       *pool;
    int              state;

    int              dev_fd;

#if defined(ML_CAMERA_HAS_V4L2)
    ml_camera_v4l2_buf_t *bufs;
    int              num_bufs;
    int              dma_buf;
    int              actual_w;
    int              actual_h;
    int              actual_pixfmt;
#endif

#if defined(ML_CAMERA_HAS_RTSP)
    AVFormatContext *fmt_ctx;
    const AVCodec   *decoder;
    AVCodecContext  *codec_ctx;
    AVFrame         *dec_frame;
    AVFrame         *rgb_frame;
    struct SwsContext *sws;
    int              video_idx;
    uint8_t         *rgb_buf;
    int              rgb_buf_size;
    int              hw_decoder;
    void            *rga;
    uint8_t         *rga_cvt_buf;
    int              rga_cvt_size;
    int              reconnect_delay;
    uint64_t         last_frame_time_us;
    int64_t          min_frame_us;
    int64_t          deadline_us;
#endif

    ml_camera_frame_cb  callback;
    void               *cb_data;
    pthread_t           thread;
    int                 thread_running;
    int                 interrupt_requested;

    char             err_msg[256];
};

static inline void ml_camera_set_err(ml_camera_t *cam, const char *msg)
{
    if (cam && msg) {
        strncpy(cam->err_msg, msg, sizeof(cam->err_msg) - 1);
        cam->err_msg[sizeof(cam->err_msg) - 1] = '\0';
    }
}

/* ── stub backend ─────────────────────────────────────────────────────── */

ml_camera_t *ml_camera_stub_create(const ml_camera_cfg_t *cfg, ml_pool_t *pool);
void         ml_camera_stub_destroy(ml_camera_t *cam);
int          ml_camera_stub_start(ml_camera_t *cam);
int          ml_camera_stub_stop(ml_camera_t *cam);
int          ml_camera_stub_read(ml_camera_t *cam, ml_frame_t *frame, int timeout_ms);
void         ml_camera_stub_frame_release(ml_camera_t *cam, ml_frame_t *frame);
int          ml_camera_stub_set_callback(ml_camera_t *cam, ml_camera_frame_cb cb, void *data);
int          ml_camera_stub_get_width(ml_camera_t *cam);
int          ml_camera_stub_get_height(ml_camera_t *cam);
int          ml_camera_stub_get_fps(ml_camera_t *cam);
int          ml_camera_stub_get_state(ml_camera_t *cam);
const char  *ml_camera_stub_error(ml_camera_t *cam);

/* ── v4l2 backend (Linux only) ────────────────────────────────────────── */

ml_camera_t *ml_camera_v4l2_create(const ml_camera_cfg_t *cfg, ml_pool_t *pool);
void         ml_camera_v4l2_destroy(ml_camera_t *cam);
int          ml_camera_v4l2_start(ml_camera_t *cam);
int          ml_camera_v4l2_stop(ml_camera_t *cam);
int          ml_camera_v4l2_read(ml_camera_t *cam, ml_frame_t *frame, int timeout_ms);
void         ml_camera_v4l2_frame_release(ml_camera_t *cam, ml_frame_t *frame);
int          ml_camera_v4l2_set_callback(ml_camera_t *cam, ml_camera_frame_cb cb, void *data);
int          ml_camera_v4l2_get_width(ml_camera_t *cam);
int          ml_camera_v4l2_get_height(ml_camera_t *cam);
int          ml_camera_v4l2_get_fps(ml_camera_t *cam);
int          ml_camera_v4l2_get_state(ml_camera_t *cam);
const char  *ml_camera_v4l2_error(ml_camera_t *cam);

/* ── rtsp backend (Linux + FFmpeg) ────────────────────────────────────── */

ml_camera_t *ml_camera_rtsp_create(const ml_camera_cfg_t *cfg, ml_pool_t *pool);
void         ml_camera_rtsp_destroy(ml_camera_t *cam);
int          ml_camera_rtsp_start(ml_camera_t *cam);
int          ml_camera_rtsp_stop(ml_camera_t *cam);
int          ml_camera_rtsp_read(ml_camera_t *cam, ml_frame_t *frame, int timeout_ms);
void         ml_camera_rtsp_frame_release(ml_camera_t *cam, ml_frame_t *frame);
int          ml_camera_rtsp_set_callback(ml_camera_t *cam, ml_camera_frame_cb cb, void *data);
int          ml_camera_rtsp_get_width(ml_camera_t *cam);
int          ml_camera_rtsp_get_height(ml_camera_t *cam);
int          ml_camera_rtsp_get_fps(ml_camera_t *cam);
int          ml_camera_rtsp_get_state(ml_camera_t *cam);
const char  *ml_camera_rtsp_error(ml_camera_t *cam);

#ifdef __cplusplus
}
#endif

#endif /* __ML_CAMERA_PRIVATE_H__ */
