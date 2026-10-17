/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_camera_rtsp.c — RTSP network camera backend.
 *
 * Uses FFmpeg libavformat/libavcodec to pull H.264/H.265 streams
 * from RTSP cameras.  On Rockchip platforms, tries h264_rkmpp for
 * hardware decoding and RGA for NV12→BGR color conversion.
 *
 * Features:
 *   - h264_rkmpp hardware decode with software fallback
 *   - RGA-accelerated NV12→BGR color conversion (with swscale fallback)
 *   - Low-latency RTSP options (nobuffer, low_delay, max_delay)
 *   - Auto-reconnect with exponential backoff
 *   - Optional FPS limiting
 */

#include "ml_camera_private.h"
#include "ml_frame.h"
#include "ml_pool.h"
#include "ml_rga.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── internal struct ──────────────────────────────────────────────────── */

/* ── forward decl ─────────────────────────────────────────────────────── */

static void *rtsp_thread(void *arg);

/* ── RGA color conversion (NV12 → BGR) ────────────────────────────────── */

static int rga_nv12_to_bgr(ml_camera_t *cam)
{
    int w = cam->codec_ctx->width;
    int h = cam->codec_ctx->height;
    int bgr_size = w * h * 3;

    /* allocate BGR output buffer if needed */
    if (!cam->rga_cvt_buf || cam->rga_cvt_size < bgr_size) {
        av_free(cam->rga_cvt_buf);
        cam->rga_cvt_buf = av_malloc(bgr_size);
        if (!cam->rga_cvt_buf) return -1;
        cam->rga_cvt_size = bgr_size;
    }

    /* build continuous NV12 buffer from AVFrame planes */
    int y_size  = cam->dec_frame->linesize[0] * h;
    int uv_size = cam->dec_frame->linesize[1] * h / 2;

    /* RGA works with continuous buffers — copy Y and UV */
    uint8_t *nv12_buf = (uint8_t *)av_malloc(y_size + uv_size);
    if (!nv12_buf) return -1;

    int i;
    for (i = 0; i < h; i++)
        memcpy(nv12_buf + i * w,
               cam->dec_frame->data[0] + i * cam->dec_frame->linesize[0], w);
    for (i = 0; i < h / 2; i++)
        memcpy(nv12_buf + y_size + i * w,
               cam->dec_frame->data[1] + i * cam->dec_frame->linesize[1], w);

    int ret = ml_rga_csc(cam->rga,
                         nv12_buf, w, h, ML_RGA_FMT_NV12,
                         cam->rga_cvt_buf, ML_RGA_FMT_BGR888);
    av_free(nv12_buf);
    return ret;
}

/* ── FFmpeg interrupt callback for timeout ───────────────────────────── */

static int rtsp_interrupt_cb(void *opaque)
{
    ml_camera_t *cam = (ml_camera_t *)opaque;
    struct timespec now;

    /* stop() requested thread exit — always interrupt */
    if (cam->interrupt_requested) return 1;

    /* deadline of 0 means infinite wait (used by sync read) */
    if (cam->deadline_us == 0) return 0;

    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t now_us = now.tv_sec * 1000000LL + now.tv_nsec / 1000;
    return (now_us > cam->deadline_us) ? 1 : 0;
}

/* open codec and find video stream */
static int rtsp_open(ml_camera_t *cam)
{
    AVDictionary *opts = NULL;
    AVCodecParameters *codecpar;
    int ret;

    /* low-latency RTSP options */
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "stimeout",       "3000000", 0);
    av_dict_set(&opts, "fflags",         "nobuffer", 0);
    av_dict_set(&opts, "flags",          "low_delay", 0);
    av_dict_set(&opts, "max_delay",      "50000", 0);
    av_dict_set(&opts, "analyzeduration","1000000", 0);
    av_dict_set(&opts, "buffer_size",    "102400", 0);

    /* pre-allocate context so we can set interrupt callback */
    cam->fmt_ctx = avformat_alloc_context();
    if (!cam->fmt_ctx) {
        ml_camera_set_err(cam, "avformat_alloc_context failed");
        return -1;
    }
    cam->fmt_ctx->interrupt_callback.callback = rtsp_interrupt_cb;
    cam->fmt_ctx->interrupt_callback.opaque   = cam;

    ret = avformat_open_input(&cam->fmt_ctx, cam->cfg.device, NULL, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        cam->fmt_ctx = NULL; /* avformat_open_input frees on failure */
        ml_camera_set_err(cam, "avformat_open_input failed");
        return -1;
    }

    ret = avformat_find_stream_info(cam->fmt_ctx, NULL);
    if (ret < 0) {
        ml_camera_set_err(cam, "avformat_find_stream_info failed");
        return -1;
    }

    cam->video_idx = av_find_best_stream(cam->fmt_ctx, AVMEDIA_TYPE_VIDEO,
                                         -1, -1, NULL, 0);
    if (cam->video_idx < 0) {
        ml_camera_set_err(cam, "no video stream found");
        return -1;
    }

    codecpar = cam->fmt_ctx->streams[cam->video_idx]->codecpar;

    /* select hardware decoder by codec type */
    cam->hw_decoder = 0;
    cam->decoder    = NULL;

    if (codecpar->codec_id == AV_CODEC_ID_H264)
        cam->decoder = avcodec_find_decoder_by_name("h264_rkmpp");
    else if (codecpar->codec_id == AV_CODEC_ID_HEVC)
        cam->decoder = avcodec_find_decoder_by_name("hevc_rkmpp");

    if (cam->decoder) {
        cam->hw_decoder = 1;
    } else {
        cam->decoder = avcodec_find_decoder(codecpar->codec_id);
        cam->hw_decoder = 0;
    }

    if (!cam->decoder) {
        ml_camera_set_err(cam, "no decoder found");
        return -1;
    }

    cam->codec_ctx = avcodec_alloc_context3(cam->decoder);
    if (!cam->codec_ctx) return -1;

    /* multi-threaded decoding for software path */
    if (!cam->hw_decoder) {
        cam->codec_ctx->thread_count = 4;
        cam->codec_ctx->thread_type  = FF_THREAD_SLICE;
    }

    ret = avcodec_parameters_to_context(cam->codec_ctx, codecpar);
    if (ret < 0) {
        ml_camera_set_err(cam, "avcodec_parameters_to_context failed");
        return -1;
    }

    /* low-latency flags for software decoder */
    if (!cam->hw_decoder)
        cam->codec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;

    ret = avcodec_open2(cam->codec_ctx, cam->decoder, NULL);
    if (ret < 0 && cam->hw_decoder) {
        /* hardware decoder failed — fall back to software */
        cam->hw_decoder = 0;
        cam->decoder    = avcodec_find_decoder(codecpar->codec_id);
        if (!cam->decoder) {
            ml_camera_set_err(cam, "no software decoder fallback");
            return -1;
        }

        avcodec_free_context(&cam->codec_ctx);
        cam->codec_ctx = avcodec_alloc_context3(cam->decoder);
        if (!cam->codec_ctx) return -1;

        cam->codec_ctx->thread_count = 4;
        cam->codec_ctx->thread_type  = FF_THREAD_SLICE;

        ret = avcodec_parameters_to_context(cam->codec_ctx, codecpar);
        if (ret < 0) {
            ml_camera_set_err(cam, "avcodec_parameters_to_context failed");
            return -1;
        }

        cam->codec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
        ret = avcodec_open2(cam->codec_ctx, cam->decoder, NULL);
    }

    if (ret < 0) {
        ml_camera_set_err(cam, "avcodec_open2 failed");
        return -1;
    }

    cam->dec_frame = av_frame_alloc();
    cam->rgb_frame = av_frame_alloc();
    if (!cam->dec_frame || !cam->rgb_frame) return -1;

    /* BGR888 output buffer (for swscale fallback) */
    cam->rgb_buf_size = av_image_get_buffer_size(
        AV_PIX_FMT_BGR24,
        cam->codec_ctx->width,
        cam->codec_ctx->height, 1);
    cam->rgb_buf = av_malloc(cam->rgb_buf_size);
    if (!cam->rgb_buf) return -1;

    av_image_fill_arrays(cam->rgb_frame->data, cam->rgb_frame->linesize,
                         cam->rgb_buf, AV_PIX_FMT_BGR24,
                         cam->codec_ctx->width, cam->codec_ctx->height, 1);

    /* swscale context (used for software decode, or hw decode fallback) */
    cam->sws = sws_getContext(
        cam->codec_ctx->width, cam->codec_ctx->height,
        cam->codec_ctx->pix_fmt,
        cam->codec_ctx->width, cam->codec_ctx->height,
        AV_PIX_FMT_BGR24, SWS_BILINEAR, NULL, NULL, NULL);
    if (!cam->sws) return -1;

    /* try RGA for hardware NV12→BGR when using h264_rkmpp */
    if (cam->hw_decoder && ml_rga_available()) {
        cam->rga = ml_rga_create(cam->pool);
        if (!cam->rga) {
            /* RGA create failed — will use swscale fallback */
        }
    }

    /* FPS limiting */
    if (cam->cfg.fps_num > 0 && cam->cfg.fps_den > 0) {
        cam->min_frame_us = (int64_t)cam->cfg.fps_den * 1000000 /
                            cam->cfg.fps_num;
    } else {
        cam->min_frame_us = 0;
    }

    cam->reconnect_delay = 1;
    return 0;
}

static void rtsp_close(ml_camera_t *cam)
{
    if (cam->rga)         { ml_rga_destroy(cam->rga); cam->rga = NULL; }
    av_free(cam->rga_cvt_buf);
    cam->rga_cvt_buf  = NULL;
    cam->rga_cvt_size = 0;

    if (cam->sws)         { sws_freeContext(cam->sws); cam->sws = NULL; }
    if (cam->rgb_buf)     { av_free(cam->rgb_buf); cam->rgb_buf = NULL; }
    if (cam->rgb_frame)   { av_frame_free(&cam->rgb_frame); }
    if (cam->dec_frame)   { av_frame_free(&cam->dec_frame); }
    if (cam->codec_ctx)   { avcodec_free_context(&cam->codec_ctx); }
    if (cam->fmt_ctx)     { avformat_close_input(&cam->fmt_ctx); }
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

ml_camera_t *
ml_camera_rtsp_create(const ml_camera_cfg_t *cfg, ml_pool_t *pool)
{
    ml_camera_t *cam;

    if (!cfg || !cfg->device) return NULL;

    cam = calloc(1, sizeof(*cam));
    if (!cam) return NULL;

    cam->backend = ML_CAMERA_BACKEND_RTSP;
    cam->cfg     = *cfg;
    cam->pool    = pool;
    cam->state   = ML_CAMERA_STATE_STOPPED;

    if (rtsp_open(cam) < 0) {
        rtsp_close(cam);
        free(cam);
        return NULL;
    }

    return cam;
}

void
ml_camera_rtsp_destroy(ml_camera_t *cam)
{
    if (!cam) return;

    ml_camera_rtsp_stop(cam);
    rtsp_close(cam);
    free(cam);
}

/* ── start / stop ─────────────────────────────────────────────────────── */

int ml_camera_rtsp_start(ml_camera_t *cam)
{
    if (!cam) return -1;
    cam->state = ML_CAMERA_STATE_RUNNING;

    if (cam->callback) {
        cam->thread_running = 1;
        if (pthread_create(&cam->thread, NULL, rtsp_thread, cam) != 0) {
            cam->thread_running = 0;
        }
    }
    return 0;
}

int ml_camera_rtsp_stop(ml_camera_t *cam)
{
    if (!cam) return -1;

    if (cam->thread_running) {
        cam->thread_running = 0;
        cam->interrupt_requested = 1;
        pthread_join(cam->thread, NULL);
        cam->interrupt_requested = 0;
    }

    cam->state = ML_CAMERA_STATE_STOPPED;
    return 0;
}

/* ── synchronous read ─────────────────────────────────────────────────── */

static int rtsp_read_internal(ml_camera_t *cam, ml_frame_t *frame)
{
    AVPacket *pkt = NULL;
    int ret, got_frame = 0;

    while (!got_frame) {
        pkt = av_packet_alloc();
        if (!pkt) return -1;

        ret = av_read_frame(cam->fmt_ctx, pkt);
        if (ret < 0) {
            av_packet_free(&pkt);
            return -1;
        }

        if (pkt->stream_index == cam->video_idx) {
            ret = avcodec_send_packet(cam->codec_ctx, pkt);
            if (ret == 0) {
                ret = avcodec_receive_frame(cam->codec_ctx, cam->dec_frame);
                if (ret == 0) got_frame = 1;
            }
        }
        av_packet_free(&pkt);
    }

    /* color conversion: RGA (hw) preferred, swscale fallback */
    if (cam->rga) {
        if (rga_nv12_to_bgr(cam) == 0) {
            ml_frame_from_mem(
                frame,
                cam->rga_cvt_buf, cam->rga_cvt_size,
                cam->codec_ctx->width, cam->codec_ctx->height,
                ML_PIXFMT_BGR888,
                cam->dec_frame->pts, 0);
            return 0;
        }
        /* RGA failed — fall through to swscale */
    }

    /* swscale fallback */
    sws_scale(cam->sws,
              (const uint8_t * const *)cam->dec_frame->data,
              cam->dec_frame->linesize, 0,
              cam->codec_ctx->height,
              cam->rgb_frame->data, cam->rgb_frame->linesize);

    ml_frame_from_mem(
        frame,
        cam->rgb_buf, cam->rgb_buf_size,
        cam->codec_ctx->width, cam->codec_ctx->height,
        ML_PIXFMT_BGR888,
        cam->dec_frame->pts, 0);

    return 0;
}

int
ml_camera_rtsp_read(ml_camera_t *cam, ml_frame_t *frame, int timeout_ms)
{
    int ret;

    if (!cam || !frame) return -1;
    if (cam->state != ML_CAMERA_STATE_RUNNING) return -1;

    /* set interrupt deadline for blocking FFmpeg I/O */
    if (timeout_ms > 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        cam->deadline_us = now.tv_sec * 1000000LL +
                           now.tv_nsec / 1000 + timeout_ms * 1000LL;
    } else {
        cam->deadline_us = 0; /* no timeout */
    }

    /* FPS limiting: sleep to maintain target frame rate */
    if (cam->min_frame_us > 0 && cam->last_frame_time_us > 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t now_us = now.tv_sec * 1000000LL + now.tv_nsec / 1000;
        int64_t elapsed = now_us - (int64_t)cam->last_frame_time_us;
        if (elapsed < cam->min_frame_us) {
            usleep((useconds_t)(cam->min_frame_us - elapsed));
        }
    }

    ret = rtsp_read_internal(cam, frame);
    if (ret < 0) {
        /* auto-reconnect */
        rtsp_close(cam);
        cam->state = ML_CAMERA_STATE_ERROR;

        sleep(cam->reconnect_delay);
        if (cam->reconnect_delay < 30)
            cam->reconnect_delay *= 2;

        if (rtsp_open(cam) == 0) {
            cam->state = ML_CAMERA_STATE_RUNNING;
            cam->reconnect_delay = 1;
        }
        return -1;
    }

    /* record wall-clock delivery time for FPS throttle */
    {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        cam->last_frame_time_us = (uint64_t)(now.tv_sec * 1000000LL +
                                             now.tv_nsec / 1000);
    }
    return 0;
}

void
ml_camera_rtsp_frame_release(ml_camera_t *cam, ml_frame_t *frame)
{
    (void)cam;
    ml_frame_unref(frame);
}

/* ── async callback ───────────────────────────────────────────────────── */

static void *rtsp_thread(void *arg)
{
    ml_camera_t *cam = (ml_camera_t *)arg;
    ml_frame_t frame;

    while (cam->thread_running) {
        /* use finite timeout so av_read_frame can be interrupted
         * by the interrupt callback when stop() sets interrupt_requested */
        if (ml_camera_rtsp_read(cam, &frame, 5000) < 0)
            continue;

        if (cam->callback) {
            if (cam->callback(&frame, cam->cb_data) < 0) {
                ml_camera_rtsp_frame_release(cam, &frame);
                break;
            }
        }

        ml_camera_rtsp_frame_release(cam, &frame);
    }

    return NULL;
}

int
ml_camera_rtsp_set_callback(ml_camera_t *cam, ml_camera_frame_cb cb, void *data)
{
    if (!cam) return -1;
    if (cam->state != ML_CAMERA_STATE_STOPPED) return -1;

    cam->callback = cb;
    cam->cb_data  = data;
    return 0;
}

/* ── query ────────────────────────────────────────────────────────────── */

int ml_camera_rtsp_get_width(ml_camera_t *cam)
{
    return (cam && cam->codec_ctx) ? cam->codec_ctx->width : -1;
}

int ml_camera_rtsp_get_height(ml_camera_t *cam)
{
    return (cam && cam->codec_ctx) ? cam->codec_ctx->height : -1;
}

int ml_camera_rtsp_get_fps(ml_camera_t *cam)
{
    return cam ? cam->cfg.fps_num / (cam->cfg.fps_den ? cam->cfg.fps_den : 1) : -1;
}

int ml_camera_rtsp_get_state(ml_camera_t *cam)
{
    return cam ? cam->state : -1;
}

const char *
ml_camera_rtsp_error(ml_camera_t *cam)
{
    if (!cam || !cam->err_msg[0]) return NULL;
    return cam->err_msg;
}
