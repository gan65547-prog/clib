/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_camera.c — Camera dispatcher.
 *
 * Routes public ml_camera_* calls to the correct backend based on
 * cfg->backend at create time and cam->backend for subsequent ops.
 * Each backend implements the internal ml_camera_{v4l2,rtsp,stub}_*
 * functions declared in ml_camera_private.h.
 */

#include "ml_camera_private.h"

typedef struct { int backend; } ml_camera_hdr_t;

#define CAM_BACKEND(c) (((ml_camera_hdr_t *)(c))->backend)

/* ── lifecycle ────────────────────────────────────────────────────────── */

ml_camera_t *
ml_camera_create(const ml_camera_cfg_t *cfg, ml_pool_t *pool)
{
    if (!cfg) return NULL;

    switch (cfg->backend) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_create(cfg, pool);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_create(cfg, pool);
#endif
    case ML_CAMERA_BACKEND_FILE:
    case ML_CAMERA_BACKEND_IMAGE:
    default:
        return ml_camera_stub_create(cfg, pool);
    }
}

void
ml_camera_destroy(ml_camera_t *cam)
{
    if (!cam) return;

    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: ml_camera_v4l2_destroy(cam); break;
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: ml_camera_rtsp_destroy(cam); break;
#endif
    default: ml_camera_stub_destroy(cam); break;
    }
}

/* ── capture control ──────────────────────────────────────────────────── */

int ml_camera_start(ml_camera_t *cam)
{
    if (!cam) return -1;

    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_start(cam);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_start(cam);
#endif
    default: return ml_camera_stub_start(cam);
    }
}

int ml_camera_stop(ml_camera_t *cam)
{
    if (!cam) return -1;

    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_stop(cam);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_stop(cam);
#endif
    default: return ml_camera_stub_stop(cam);
    }
}

/* ── synchronous read ─────────────────────────────────────────────────── */

int ml_camera_read(ml_camera_t *cam, ml_frame_t *frame, int timeout_ms)
{
    if (!cam) return -1;

    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_read(cam, frame, timeout_ms);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_read(cam, frame, timeout_ms);
#endif
    default: return ml_camera_stub_read(cam, frame, timeout_ms);
    }
}

void ml_camera_frame_release(ml_camera_t *cam, ml_frame_t *frame)
{
    if (!cam) return;

    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: ml_camera_v4l2_frame_release(cam, frame); break;
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: ml_camera_rtsp_frame_release(cam, frame); break;
#endif
    default: ml_camera_stub_frame_release(cam, frame); break;
    }
}

/* ── async callback mode ──────────────────────────────────────────────── */

int ml_camera_set_callback(ml_camera_t *cam, ml_camera_frame_cb cb, void *data)
{
    if (!cam) return -1;

    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_set_callback(cam, cb, data);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_set_callback(cam, cb, data);
#endif
    default: return ml_camera_stub_set_callback(cam, cb, data);
    }
}

/* ── query ────────────────────────────────────────────────────────────── */

int ml_camera_get_width(ml_camera_t *cam)
{
    if (!cam) return -1;
    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_get_width(cam);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_get_width(cam);
#endif
    default: return ml_camera_stub_get_width(cam);
    }
}

int ml_camera_get_height(ml_camera_t *cam)
{
    if (!cam) return -1;
    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_get_height(cam);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_get_height(cam);
#endif
    default: return ml_camera_stub_get_height(cam);
    }
}

int ml_camera_get_fps(ml_camera_t *cam)
{
    if (!cam) return -1;
    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_get_fps(cam);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_get_fps(cam);
#endif
    default: return ml_camera_stub_get_fps(cam);
    }
}

int ml_camera_get_state(ml_camera_t *cam)
{
    if (!cam) return -1;
    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_get_state(cam);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_get_state(cam);
#endif
    default: return ml_camera_stub_get_state(cam);
    }
}

const char *ml_camera_error(ml_camera_t *cam)
{
    if (!cam) return NULL;
    switch (CAM_BACKEND(cam)) {
#ifdef ML_CAMERA_HAS_V4L2
    case ML_CAMERA_BACKEND_V4L2: return ml_camera_v4l2_error(cam);
#endif
#ifdef ML_CAMERA_HAS_RTSP
    case ML_CAMERA_BACKEND_RTSP: return ml_camera_rtsp_error(cam);
#endif
    default: return ml_camera_stub_error(cam);
    }
}
