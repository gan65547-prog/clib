/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_codec — Hardware video encoder abstraction.
 *
 * Wraps Rockchip MPP (Media Process Platform) for H.264/H.265 encoding
 * with DMA-BUF zero-copy support.  Falls back to VAAPI / NVCODEC / software
 * encoders where MPP is unavailable.
 *
 * Lifecycle:
 *   create → send_frame* → receive_packet* → flush → destroy
 *
 * Input:  NV12 / NV21 ml_frame_t (caller uses ml_rga for BGR→NV12)
 * Output: H.264 / H.265 NAL units via ml_codec_pkt_t
 */

#ifndef __ML_CODEC_H__
#define __ML_CODEC_H__

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

/* error codes */
#define ML_EAGAIN  (-2)   /* try again later */

/* ── forward declarations ─────────────────────────────────────────────── */

typedef struct ml_frame_s ml_frame_t;
typedef struct ml_pool_s  ml_pool_t;
typedef struct ml_codec_s ml_codec_t;

/* ── enums ────────────────────────────────────────────────────────────── */

typedef enum {
    ML_CODEC_BACKEND_MPP      = 0,  /* Rockchip MPP (RK3588)      */
    ML_CODEC_BACKEND_VAAPI    = 1,  /* VAAPI (Intel/AMD GPU)     */
    ML_CODEC_BACKEND_NVCODEC  = 2,  /* NVIDIA NVCODEC (Jetson)   */
    ML_CODEC_BACKEND_SOFTWARE = 3,  /* x264/x265 software fallback */
} ml_codec_backend_t;

typedef enum {
    ML_CODEC_H264  = 0,
    ML_CODEC_H265  = 1,
    ML_CODEC_MJPEG = 2,
} ml_codec_enc_type_t;

typedef enum {
    ML_CODEC_RC_CBR   = 0,  /* constant bitrate    */
    ML_CODEC_RC_VBR   = 1,  /* variable bitrate    */
    ML_CODEC_RC_AVBR  = 2,  /* average VBR         */
    ML_CODEC_RC_FIXQP = 3,  /* fixed QP (quality)  */
} ml_codec_rc_mode_t;

typedef enum {
    ML_CODEC_PROFILE_H264_BASELINE = 0,
    ML_CODEC_PROFILE_H264_MAIN     = 1,
    ML_CODEC_PROFILE_H264_HIGH     = 2,
    ML_CODEC_PROFILE_H265_MAIN     = 3,
    ML_CODEC_PROFILE_H265_MAIN10   = 4,
} ml_codec_profile_t;

/* ── configuration ────────────────────────────────────────────────────── */

typedef struct {
    ml_codec_backend_t   backend;
    ml_codec_enc_type_t  enc_type;
    int                  width;
    int                  height;
    int                  fps_num;
    int                  fps_den;
    int                  bitrate;        /* bps, e.g. 4000000 for 4 Mbps  */
    ml_codec_rc_mode_t   rc_mode;
    int                  gop;            /* I-frame interval               */
    ml_codec_profile_t   profile;
    int                  qp_init;        /* initial QP (0 = auto)          */
    int                  qp_min;         /* min QP for P/B frames          */
    int                  qp_max;         /* max QP for P/B frames          */
    int                  qp_max_i;       /* max QP for I frames            */
    int                  qp_min_i;       /* min QP for I frames            */
    int                  qp_ip;          /* QP delta between I and P       */
    int                  quality;        /* 0-100, software encoders only  */
    int                  gop_mode;       /* 0=normal, 1-4=smart GOP mode   */
    int                  vi_len;         /* virtual I-frame interval        */
} ml_codec_cfg_t;

/* defaults */
#define ML_CODEC_CFG_DEFAULT {              \
    ML_CODEC_BACKEND_MPP,                   \
    ML_CODEC_H264,                          \
    1920, 1080,                             \
    30, 1,                                  \
    4000000,                                \
    ML_CODEC_RC_CBR,                        \
    60,                                     \
    ML_CODEC_PROFILE_H264_HIGH,              \
    -1, 20, 30,                             \
    30, 20,                                 \
    2,                                      \
    0,                                      \
    0, 0                                    \
}

/* ── encoded packet ───────────────────────────────────────────────────── */

typedef struct {
    uint8_t *data;
    int      size;
    int64_t  pts;          /* ms                          */
    int64_t  dts;          /* ms                          */
    int      keyframe;     /* 1 = IDR / I-frame           */
    int      frame_type;   /* 0=I, 1=P, 2=B               */
} ml_codec_pkt_t;

/* ── lifecycle ────────────────────────────────────────────────────────── */

/* Create encoder instance. cfg can be NULL for defaults.
 * Returns NULL on failure. */
ml_codec_t *ml_codec_create(const ml_codec_cfg_t *cfg, ml_pool_t *pool);

/* Destroy encoder. Safe on NULL. */
void        ml_codec_destroy(ml_codec_t *c);

/* ── encoding ─────────────────────────────────────────────────────────── */

/* Send a raw frame for encoding. Frame must be NV12 or NV21.
 * Non-blocking: returns 0 on success, -1 if encoder is busy or error. */
int  ml_codec_send_frame(ml_codec_t *c, ml_frame_t *frame);

/* Receive an encoded packet. Non-blocking.
 * Returns 0 and fills *pkt on success.
 * Returns ML_EAGAIN if no packet ready.
 * Returns -1 on error. */
int  ml_codec_receive_packet(ml_codec_t *c, ml_codec_pkt_t *pkt);

/* Flush remaining packets after last frame. Call until it returns 0.
 * Returns 0 when drain is complete, >0 if a packet was flushed (check *pkt),
 * -1 on error. */
int  ml_codec_flush(ml_codec_t *c);

/* ── extradata ────────────────────────────────────────────────────────── */

/* Get codec-private data (SPS+PPS for H.264, VPS+SPS+PPS for H.265).
 * Data is valid until the next send_frame call or destroy.
 * Returns 0 on success, -1 if extradata not yet available. */
int  ml_codec_get_extradata(ml_codec_t *c, uint8_t **data, size_t *size);

/* ── query ────────────────────────────────────────────────────────────── */

/* Last error message. Returns NULL if no error. */
const char *ml_codec_error(ml_codec_t *c);

/* Return 1 if the given backend is available on this system. */
int  ml_codec_backend_available(ml_codec_backend_t backend);

#endif /* __ML_CODEC_H__ */
