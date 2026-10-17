/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_camera_v4l2.c — V4L2 camera backend (USB / MIPI).
 *
 * Compiles only on Linux.  Implements DMA-BUF zero-copy with MMAP
 * fallback.  Accepts YUYV and MJPEG from camera; callers should use
 * ml_rga to convert to NV12 or RGB as needed.
 */

#include "ml_camera_private.h"
#include "ml_frame.h"
#include "ml_pool.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

/* ── helpers ──────────────────────────────────────────────────────────── */

static int xioctl(int fd, unsigned long req, void *arg)
{
    int r;
    do { r = ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
    return r;
}

/* convert V4L2 pixel format to ml_pixfmt_t */
static int v4l2_to_ml_pixfmt(uint32_t v4l2_fmt)
{
    switch (v4l2_fmt) {
    case V4L2_PIX_FMT_BGR24:   return ML_PIXFMT_BGR888;
    case V4L2_PIX_FMT_RGB24:   return ML_PIXFMT_RGB888;
    case V4L2_PIX_FMT_NV12:    return ML_PIXFMT_NV12;
    case V4L2_PIX_FMT_NV21:    return ML_PIXFMT_NV21;
    case V4L2_PIX_FMT_YUYV:    return ML_PIXFMT_YUYV;
    case V4L2_PIX_FMT_MJPEG:   return ML_PIXFMT_MJPEG;
    default:                   return -1;
    }
}

static uint32_t ml_to_v4l2_pixfmt(int ml_fmt)
{
    switch (ml_fmt) {
    case ML_PIXFMT_BGR888: return V4L2_PIX_FMT_BGR24;
    case ML_PIXFMT_RGB888: return V4L2_PIX_FMT_RGB24;
    case ML_PIXFMT_NV12:   return V4L2_PIX_FMT_NV12;
    case ML_PIXFMT_NV21:   return V4L2_PIX_FMT_NV21;
    case ML_PIXFMT_YUYV:   return V4L2_PIX_FMT_YUYV;
    case ML_PIXFMT_MJPEG:  return V4L2_PIX_FMT_MJPEG;
    default:               return 0;
    }
}

/* ── V4L2 release callback ────────────────────────────────────────────── */

static void v4l2_dmabuf_release(void *owner, int fd)
{
    (void)owner;
    if (fd >= 0) close(fd);
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

ml_camera_t *
ml_camera_v4l2_create(const ml_camera_cfg_t *cfg, ml_pool_t *pool)
{
    ml_camera_t *cam;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    uint32_t v4l2_fmt;
    int i;

    if (!cfg || !cfg->device) return NULL;

    cam = calloc(1, sizeof(*cam));
    if (!cam) return NULL;

    cam->backend = ML_CAMERA_BACKEND_V4L2;
    cam->cfg     = *cfg;
    cam->pool    = pool;
    cam->dev_fd  = -1;
    cam->state   = ML_CAMERA_STATE_STOPPED;

    /* open device */
    cam->dev_fd = open(cfg->device, O_RDWR | O_NONBLOCK);
    if (cam->dev_fd < 0) {
        ml_camera_set_err(cam, "open device failed");
        free(cam);
        return NULL;
    }

    /* query capabilities */
    memset(&cap, 0, sizeof(cap));
    if (xioctl(cam->dev_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        ml_camera_set_err(cam, "VIDIOC_QUERYCAP failed");
        close(cam->dev_fd);
        free(cam);
        return NULL;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ml_camera_set_err(cam, "not a video capture device");
        close(cam->dev_fd);
        free(cam);
        return NULL;
    }

    /* set format */
    v4l2_fmt = ml_to_v4l2_pixfmt(cfg->pixfmt);
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = cfg->width;
    fmt.fmt.pix.height      = cfg->height;
    fmt.fmt.pix.pixelformat = v4l2_fmt ? v4l2_fmt : V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    if (xioctl(cam->dev_fd, VIDIOC_S_FMT, &fmt) < 0) {
        ml_camera_set_err(cam, "VIDIOC_S_FMT failed");
        close(cam->dev_fd);
        free(cam);
        return NULL;
    }

    cam->actual_w      = fmt.fmt.pix.width;
    cam->actual_h      = fmt.fmt.pix.height;
    cam->actual_pixfmt = v4l2_to_ml_pixfmt(fmt.fmt.pix.pixelformat);

    /* request buffers (MMAP) */
    memset(&req, 0, sizeof(req));
    req.count  = cfg->buffer_count > 0 ? cfg->buffer_count : 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(cam->dev_fd, VIDIOC_REQBUFS, &req) < 0) {
        ml_camera_set_err(cam, "VIDIOC_REQBUFS failed");
        close(cam->dev_fd);
        free(cam);
        return NULL;
    }

    cam->num_bufs = req.count;
    cam->bufs = calloc(cam->num_bufs, sizeof(ml_camera_v4l2_buf_t));
    if (!cam->bufs) {
        close(cam->dev_fd);
        free(cam);
        return NULL;
    }

    /* mmap each buffer */
    for (i = 0; i < cam->num_bufs; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (xioctl(cam->dev_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            ml_camera_set_err(cam, "VIDIOC_QUERYBUF failed");
            ml_camera_v4l2_destroy(cam);
            return NULL;
        }

        cam->bufs[i].length = buf.length;
        cam->bufs[i].start = mmap(NULL, buf.length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED, cam->dev_fd,
                                  buf.m.offset);
        if (cam->bufs[i].start == MAP_FAILED) {
            ml_camera_set_err(cam, "mmap failed");
            ml_camera_v4l2_destroy(cam);
            return NULL;
        }

        /* try DMA-BUF export */
        {
            struct v4l2_exportbuffer exp;
            memset(&exp, 0, sizeof(exp));
            exp.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            exp.index = i;
            exp.flags = O_RDWR;
            if (xioctl(cam->dev_fd, VIDIOC_EXPBUF, &exp) == 0) {
                cam->bufs[i].fd = exp.fd;
                cam->dma_buf = 1;
            } else {
                cam->bufs[i].fd = -1;
            }
        }
    }

    return cam;
}

void
ml_camera_v4l2_destroy(ml_camera_t *cam)
{
    int i;

    if (!cam) return;

    ml_camera_v4l2_stop(cam);

    if (cam->dev_fd >= 0) {
        /* unmap buffers */
        for (i = 0; i < cam->num_bufs; i++) {
            if (cam->bufs[i].start && cam->bufs[i].start != MAP_FAILED)
                munmap(cam->bufs[i].start, cam->bufs[i].length);
            if (cam->bufs[i].fd >= 0)
                close(cam->bufs[i].fd);
        }

        /* free buffers from driver */
        {
            struct v4l2_requestbuffers req;
            memset(&req, 0, sizeof(req));
            req.count  = 0;
            req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            req.memory = V4L2_MEMORY_MMAP;
            ioctl(cam->dev_fd, VIDIOC_REQBUFS, &req);
        }

        close(cam->dev_fd);
        cam->dev_fd = -1;
    }

    free(cam->bufs);
    free(cam);
}

/* ── start / stop ─────────────────────────────────────────────────────── */

int
ml_camera_v4l2_start(ml_camera_t *cam)
{
    enum v4l2_buf_type type;
    int i;

    if (!cam || cam->dev_fd < 0) return -1;
    if (cam->state == ML_CAMERA_STATE_RUNNING) return 0;

    /* QBUF all buffers */
    for (i = 0; i < cam->num_bufs; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (xioctl(cam->dev_fd, VIDIOC_QBUF, &buf) < 0) {
            ml_camera_set_err(cam, "VIDIOC_QBUF failed");
            return -1;
        }
        cam->bufs[i].queued = 1;
    }

    /* stream on */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(cam->dev_fd, VIDIOC_STREAMON, &type) < 0) {
        ml_camera_set_err(cam, "VIDIOC_STREAMON failed");
        return -1;
    }

    cam->state = ML_CAMERA_STATE_RUNNING;

    /* start async capture thread if callback was set */
    if (cam->callback) {
        cam->thread_running = 1;
        if (pthread_create(&cam->thread, NULL, camera_thread, cam) != 0) {
            cam->thread_running = 0;
            xioctl(cam->dev_fd, VIDIOC_STREAMOFF, &type);
            cam->state = ML_CAMERA_STATE_ERROR;
            ml_camera_set_err(cam, "pthread_create failed");
            return -1;
        }
    }

    return 0;
}

int
ml_camera_v4l2_stop(ml_camera_t *cam)
{
    enum v4l2_buf_type type;

    if (!cam || cam->dev_fd < 0) return -1;

    /* stop async thread first */
    if (cam->thread_running) {
        cam->thread_running = 0;
        pthread_join(cam->thread, NULL);
    }

    if (cam->state == ML_CAMERA_STATE_RUNNING) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(cam->dev_fd, VIDIOC_STREAMOFF, &type);
    }

    cam->state = ML_CAMERA_STATE_STOPPED;
    return 0;
}

/* ── synchronous read ─────────────────────────────────────────────────── */

int
ml_camera_v4l2_read(ml_camera_t *cam, ml_frame_t *frame, int timeout_ms)
{
    struct v4l2_buffer buf;
    struct timeval tv;
    fd_set fds;
    int ret;

    if (!cam || !frame || cam->dev_fd < 0) return -1;
    if (cam->state != ML_CAMERA_STATE_RUNNING) return -1;

    /* wait for data with timeout */
    if (timeout_ms >= 0) {
        FD_ZERO(&fds);
        FD_SET(cam->dev_fd, &fds);
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ret = select(cam->dev_fd + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
    }

    /* dequeue */
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(cam->dev_fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) return -1;
        ml_camera_set_err(cam, "VIDIOC_DQBUF failed");
        cam->state = ML_CAMERA_STATE_ERROR;
        return -1;
    }

    cam->bufs[buf.index].queued = 0;

    /* build ml_frame_t.
     * QBUF is handled by ml_camera_v4l2_frame_release, which knows
     * the exact buffer index via frame->priv — the release callback
     * must NOT QBUF here. */
    if (cam->dma_buf && cam->bufs[buf.index].fd >= 0) {
        ml_frame_from_dmabuf(
            frame,
            cam->bufs[buf.index].fd, -1,
            cam->bufs[buf.index].start, NULL,
            buf.bytesused, 0,
            cam->actual_w, cam->actual_h,
            cam->actual_w, 0,
            cam->actual_pixfmt,
            buf.timestamp.tv_sec * 1000000ULL + buf.timestamp.tv_usec,
            buf.sequence,
            ML_FRAME_FLAG_DMABUF,
            cam, v4l2_dmabuf_release
        );
    } else {
        ml_frame_from_mem(
            frame,
            cam->bufs[buf.index].start, buf.bytesused,
            cam->actual_w, cam->actual_h,
            cam->actual_pixfmt,
            buf.timestamp.tv_sec * 1000000ULL + buf.timestamp.tv_usec,
            buf.sequence
        );
        frame->release = NULL;
        frame->owner   = NULL;
    }

    /* store buf.index in priv for release */
    frame->priv = (void *)(intptr_t)buf.index;

    return 0;
}

void
ml_camera_v4l2_frame_release(ml_camera_t *cam, ml_frame_t *frame)
{
    int idx;
    struct v4l2_buffer buf;

    if (!cam || !frame || cam->dev_fd < 0) return;

    idx = (int)(intptr_t)frame->priv;
    if (idx < 0 || idx >= cam->num_bufs) {
        ml_frame_unref(frame);
        return;
    }
    if (cam->bufs[idx].queued) {
        ml_frame_unref(frame);
        return;
    }

    /* QBUF: return the exact buffer to the V4L2 driver queue.
     * The frame release callback (v4l2_dmabuf_release or NULL)
     * only handles DMA-BUF fd cleanup — it does NOT QBUF. */
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = idx;

    if (xioctl(cam->dev_fd, VIDIOC_QBUF, &buf) == 0)
        cam->bufs[idx].queued = 1;

    ml_frame_unref(frame);
}

/* ── async callback ───────────────────────────────────────────────────── */

static void *camera_thread(void *arg)
{
    ml_camera_t *cam = (ml_camera_t *)arg;
    ml_frame_t frame;

    while (cam->thread_running) {
        if (ml_camera_v4l2_read(cam, &frame, cam->cfg.timeout_ms) < 0)
            continue;

        if (cam->callback)
            cam->callback(&frame, cam->cb_data);

        ml_camera_v4l2_frame_release(cam, &frame);
    }

    return NULL;
}

int
ml_camera_v4l2_set_callback(ml_camera_t *cam, ml_camera_frame_cb cb, void *data)
{
    if (!cam) return -1;
    if (cam->state != ML_CAMERA_STATE_STOPPED) return -1;

    cam->callback = cb;
    cam->cb_data  = data;
    return 0;
}

/* ── query ────────────────────────────────────────────────────────────── */

int ml_camera_v4l2_get_width(ml_camera_t *cam)  { return cam ? cam->actual_w : -1; }
int ml_camera_v4l2_get_height(ml_camera_t *cam) { return cam ? cam->actual_h : -1; }
int ml_camera_v4l2_get_fps(ml_camera_t *cam)    { return cam ? cam->cfg.fps_num / (cam->cfg.fps_den ? cam->cfg.fps_den : 1) : -1; }
int ml_camera_v4l2_get_state(ml_camera_t *cam)  { return cam ? cam->state : -1; }

const char *
ml_camera_v4l2_error(ml_camera_t *cam)
{
    if (!cam || !cam->err_msg[0]) return NULL;
    return cam->err_msg;
}
