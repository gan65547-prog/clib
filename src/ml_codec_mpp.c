/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_codec_mpp.c — Rockchip MPP hardware encoder backend.
 *
 * Compiles only on Linux aarch64 with librockchip_mpp installed.
 *
 * Features:
 *   - DRM buffer group for zero-copy DMA-BUF sharing with RGA/display
 *   - CBR / VBR / AVBR / FIXQP rate control with proper bps ratios
 *   - Per-RC-mode QP configuration (qp_init/min/max/min_i/max_i/ip)
 *   - Smart GOP reference frame management
 *   - SEI per-frame, header-on-IDR, drop control
 *   - Proper stride via mpi_enc_width_default_stride
 */

#include "ml_codec.h"
#include "ml_frame.h"
#include "ml_pool.h"

#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_err.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_meta.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/mpp_rc_api.h>
#include <rockchip/mpp_task.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/rk_venc_cmd.h>
#include <rockchip/rk_venc_ref.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── internal struct ──────────────────────────────────────────────────── */

struct ml_codec_s {
    MppCtx           ctx;
    MppApi          *mpi;
    MppEncCfg        enc_cfg;

    ml_codec_cfg_t   user_cfg;
    ml_pool_t       *pool;

    /* DRM buffer group (zero-copy path) */
    MppBufferGroup   buf_grp;
    MppBuffer        frame_buf;     /* pre-allocated input frame buffer  */
    MppBuffer        pkt_buf;       /* pre-allocated output packet buffer */
    int              hor_stride;
    int              ver_stride;
    int              frame_size;

    /* extradata */
    uint8_t         *extradata;
    size_t           extradata_len;

    /* packet data buffer — copy output so caller owns the memory */
    uint8_t         *pkt_data;
    size_t           pkt_data_cap;

    /* poll mode */
    MppPollType      poll_type;

    /* error */
    char             err_msg[256];

    /* state */
    int              inited;
    pthread_mutex_t  lock;
};

/* ── helpers ──────────────────────────────────────────────────────────── */

static MppCodingType
to_mpp_coding(ml_codec_enc_type_t t)
{
    switch (t) {
    case ML_CODEC_H264:  return MPP_VIDEO_CodingAVC;
    case ML_CODEC_H265:  return MPP_VIDEO_CodingHEVC;
    case ML_CODEC_MJPEG: return MPP_VIDEO_CodingMJPEG;
    default:             return MPP_VIDEO_CodingUnused;
    }
}

static void
set_err(ml_codec_t *c, const char *msg)
{
    if (c && msg) {
        strncpy(c->err_msg, msg, sizeof(c->err_msg) - 1);
        c->err_msg[sizeof(c->err_msg) - 1] = '\0';
    }
}

/* ── DRM buffer group setup ───────────────────────────────────────────── */

static int setup_drm_buffers(ml_codec_t *c)
{
    MppRet ret;

    ret = mpp_buffer_group_get_internal(&c->buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret != MPP_OK) {
        /* DRM not available — fall back to per-frame allocation */
        c->buf_grp = NULL;
        return 0;
    }

    ret = mpp_buffer_get(c->buf_grp, &c->frame_buf, c->frame_size);
    if (ret != MPP_OK) {
        mpp_buffer_group_put(c->buf_grp);
        c->buf_grp = NULL;
        return -1;
    }

    ret = mpp_buffer_get(c->buf_grp, &c->pkt_buf, c->frame_size);
    if (ret != MPP_OK) {
        mpp_buffer_put(c->frame_buf);
        c->frame_buf = NULL;
        mpp_buffer_group_put(c->buf_grp);
        c->buf_grp = NULL;
        return -1;
    }

    return 0;
}

static void teardown_drm_buffers(ml_codec_t *c)
{
    if (c->pkt_buf)   { mpp_buffer_put(c->pkt_buf);   c->pkt_buf   = NULL; }
    if (c->frame_buf) { mpp_buffer_put(c->frame_buf); c->frame_buf = NULL; }
    if (c->buf_grp)   { mpp_buffer_group_clear(c->buf_grp);
                        mpp_buffer_group_put(c->buf_grp);
                        c->buf_grp = NULL; }
}

/* ── rate control configuration ───────────────────────────────────────── */

static void config_rc(ml_codec_t *c)
{
    MppEncCfg cfg = c->enc_cfg;
    int target = c->user_cfg.bitrate;
    int rc_mode;

    /* map RC mode */
    switch (c->user_cfg.rc_mode) {
    case ML_CODEC_RC_VBR:   rc_mode = MPP_ENC_RC_MODE_VBR;   break;
    case ML_CODEC_RC_AVBR:  rc_mode = MPP_ENC_RC_MODE_AVBR;  break;
    case ML_CODEC_RC_FIXQP: rc_mode = MPP_ENC_RC_MODE_FIXQP; break;
    default:                rc_mode = MPP_ENC_RC_MODE_CBR;   break;
    }

    mpp_enc_cfg_set_s32(cfg, "rc:mode", rc_mode);

    /* bitrate bounds per RC mode (reference: TMppEncoder.C) */
    switch (rc_mode) {
    case MPP_ENC_RC_MODE_FIXQP:
        /* FIXQP: no bitrate target — quality controlled by QP */
        break;
    case MPP_ENC_RC_MODE_CBR:
        /* CBR: narrow bound */
        mpp_enc_cfg_set_s32(cfg, "rc:bps_target", target);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_max", target * 17 / 16);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_min", target * 15 / 16);
        break;
    case MPP_ENC_RC_MODE_VBR:
    case MPP_ENC_RC_MODE_AVBR:
        /* VBR/AVBR: wide bound */
        mpp_enc_cfg_set_s32(cfg, "rc:bps_target", target);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_max", target * 17 / 16);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_min", target * 1 / 16);
        break;
    default:
        mpp_enc_cfg_set_s32(cfg, "rc:bps_target", target);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_max", target * 17 / 16);
        mpp_enc_cfg_set_s32(cfg, "rc:bps_min", target * 15 / 16);
        break;
    }
}

/* ── QP configuration per RC mode ─────────────────────────────────────── */

static void config_qp(ml_codec_t *c)
{
    MppEncCfg cfg = c->enc_cfg;
    int qp_init, qp_max, qp_min, qp_max_i, qp_min_i, qp_ip;

    /* use user-provided values if set (> 0), otherwise sensible defaults */
    if (c->user_cfg.qp_init > 0) {
        qp_init  = c->user_cfg.qp_init;
        qp_max   = c->user_cfg.qp_max;
        qp_min   = c->user_cfg.qp_min;
        qp_max_i = c->user_cfg.qp_max_i;
        qp_min_i = c->user_cfg.qp_min_i;
        qp_ip    = c->user_cfg.qp_ip;
    } else {
        /* defaults per RC mode */
        switch (c->user_cfg.rc_mode) {
        case ML_CODEC_RC_FIXQP:
            qp_init  = 20; qp_max   = 20; qp_min   = 20;
            qp_max_i = 20; qp_min_i = 20; qp_ip    = 2;
            break;
        default:
            qp_init  = 25; qp_max   = 30; qp_min   = 20;
            qp_max_i = 30; qp_min_i = 20; qp_ip    = 2;
            break;
        }
    }

    mpp_enc_cfg_set_s32(cfg, "rc:qp_init",  qp_init);
    mpp_enc_cfg_set_s32(cfg, "rc:qp_max",   qp_max);
    mpp_enc_cfg_set_s32(cfg, "rc:qp_min",   qp_min);
    mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", qp_max_i);
    mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", qp_min_i);
    mpp_enc_cfg_set_s32(cfg, "rc:qp_ip",    qp_ip);
}

/* ── smart GOP ref cfg ────────────────────────────────────────────────── */

static int config_gop_ref(ml_codec_t *c)
{
    MppEncCfg cfg = c->enc_cfg;
    int gop_mode = c->user_cfg.gop_mode;

    if (gop_mode <= 0) return 0;

    void *ref = NULL;
    MppRet ret = mpp_enc_ref_cfg_init(&ref);
    if (ret != MPP_OK) return -1;

    if (gop_mode < 4)
        mpi_enc_gen_ref_cfg(ref, gop_mode);
    else
        mpi_enc_gen_smart_gop_ref_cfg(ref, c->user_cfg.gop, c->user_cfg.vi_len);

    ret = c->mpi->control(c->ctx, MPP_ENC_SET_REF_CFG, ref);
    mpp_enc_ref_cfg_deinit(&ref);
    return (ret == MPP_OK) ? 0 : -1;
}

/* ── SEI / header / drop configuration ────────────────────────────────── */

static int config_sei_header_drop(ml_codec_t *c)
{
    MppEncSeiMode    sei_mode    = MPP_ENC_SEI_MODE_ONE_FRAME;
    MppEncHeaderMode header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
    MppRet ret;

    ret = c->mpi->control(c->ctx, MPP_ENC_SET_SEI_CFG, &sei_mode);
    if (ret != MPP_OK) return -1;

    if (c->user_cfg.enc_type == ML_CODEC_H264 ||
        c->user_cfg.enc_type == ML_CODEC_H265) {
        ret = c->mpi->control(c->ctx, MPP_ENC_SET_HEADER_MODE, &header_mode);
        if (ret != MPP_OK) return -1;
    }

    /* drop config: don't drop frames, but configure threshold */
    mpp_enc_cfg_set_u32(c->enc_cfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
    mpp_enc_cfg_set_u32(c->enc_cfg, "rc:drop_thd",  20);
    mpp_enc_cfg_set_u32(c->enc_cfg, "rc:drop_gap",  1);

    return 0;
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

ml_codec_t *
ml_codec_create(const ml_codec_cfg_t *cfg, ml_pool_t *pool)
{
    ml_codec_t *c;
    MppCodingType coding;
    MppRet ret;

    c = calloc(1, sizeof(*c));
    if (!c) return NULL;

    if (cfg)
        c->user_cfg = *cfg;
    else
        c->user_cfg = (ml_codec_cfg_t)ML_CODEC_CFG_DEFAULT;

    c->pool = pool;
    pthread_mutex_init(&c->lock, NULL);

    coding = to_mpp_coding(c->user_cfg.enc_type);
    if (coding == MPP_VIDEO_CodingUnused) {
        set_err(c, "unsupported codec type");
        free(c);
        return NULL;
    }

    /* stride: use MPP's recommended width alignment (reference: TMppEncoder) */
    c->hor_stride = mpi_enc_width_default_stride(c->user_cfg.width,
                                                  MPP_FMT_YUV420SP);
    c->ver_stride = MPP_ALIGN(c->user_cfg.height, 8);
    c->frame_size = MPP_ALIGN(c->hor_stride, 64) *
                    MPP_ALIGN(c->ver_stride, 64) * 3 / 2;

    /* DRM buffer group for zero-copy */
    setup_drm_buffers(c);

    /* create MPP context */
    ret = mpp_create(&c->ctx, &c->mpi);
    if (ret != MPP_OK) {
        set_err(c, "mpp_create failed");
        teardown_drm_buffers(c);
        free(c);
        return NULL;
    }

    /* init as encoder */
    ret = mpp_init(c->ctx, MPP_CTX_ENC, coding);
    if (ret != MPP_OK) {
        set_err(c, "mpp_init failed");
        mpp_destroy(c->ctx);
        teardown_drm_buffers(c);
        free(c);
        return NULL;
    }

    /* configure encoder */
    ret = mpp_enc_cfg_init(c->ctx, &c->enc_cfg);
    if (ret != MPP_OK) {
        set_err(c, "mpp_enc_cfg_init failed");
        mpp_destroy(c->ctx);
        teardown_drm_buffers(c);
        free(c);
        return NULL;
    }

    /* basic parameters */
    mpp_enc_cfg_set_s32(c->enc_cfg, "prep:width",      c->user_cfg.width);
    mpp_enc_cfg_set_s32(c->enc_cfg, "prep:height",     c->user_cfg.height);
    mpp_enc_cfg_set_s32(c->enc_cfg, "prep:hor_stride", c->hor_stride);
    mpp_enc_cfg_set_s32(c->enc_cfg, "prep:ver_stride", c->ver_stride);
    mpp_enc_cfg_set_s32(c->enc_cfg, "prep:format",     MPP_FMT_YUV420SP);

    /* rate control */
    config_rc(c);
    config_qp(c);

    /* GOP */
    mpp_enc_cfg_set_s32(c->enc_cfg, "rc:gop",
        c->user_cfg.gop > 0 ? c->user_cfg.gop : c->user_cfg.fps_num * 2);

    /* framerate */
    mpp_enc_cfg_set_s32(c->enc_cfg, "rc:fps_in_flex",  0);
    mpp_enc_cfg_set_s32(c->enc_cfg, "rc:fps_in_num",   c->user_cfg.fps_num);
    mpp_enc_cfg_set_s32(c->enc_cfg, "rc:fps_in_den",   c->user_cfg.fps_den);
    mpp_enc_cfg_set_s32(c->enc_cfg, "rc:fps_out_flex", 0);
    mpp_enc_cfg_set_s32(c->enc_cfg, "rc:fps_out_num",  c->user_cfg.fps_num);
    mpp_enc_cfg_set_s32(c->enc_cfg, "rc:fps_out_den",  c->user_cfg.fps_den);

    /* codec-specific */
    if (c->user_cfg.enc_type == ML_CODEC_H264) {
        mpp_enc_cfg_set_s32(c->enc_cfg, "codec:type",
                            MPP_VIDEO_CodingAVC);
        switch (c->user_cfg.profile) {
        case ML_CODEC_PROFILE_H264_BASELINE:
            mpp_enc_cfg_set_s32(c->enc_cfg, "h264:profile", 66); break;
        case ML_CODEC_PROFILE_H264_MAIN:
            mpp_enc_cfg_set_s32(c->enc_cfg, "h264:profile", 77); break;
        default:
            mpp_enc_cfg_set_s32(c->enc_cfg, "h264:profile", 100); break;
        }
        mpp_enc_cfg_set_s32(c->enc_cfg, "h264:level",   40);
        mpp_enc_cfg_set_s32(c->enc_cfg, "h264:cabac_en", 1);
        mpp_enc_cfg_set_s32(c->enc_cfg, "h264:cabac_idc", 0);
        mpp_enc_cfg_set_s32(c->enc_cfg, "h264:trans8x8", 0);
    } else if (c->user_cfg.enc_type == ML_CODEC_H265) {
        mpp_enc_cfg_set_s32(c->enc_cfg, "codec:type",
                            MPP_VIDEO_CodingHEVC);
    }

    /* split mode */
    mpp_enc_cfg_set_s32(c->enc_cfg, "split:mode", MPP_ENC_SPLIT_NONE);

    /* apply config to MPP */
    ret = c->mpi->control(c->ctx, MPP_ENC_SET_CFG, c->enc_cfg);
    if (ret != MPP_OK) {
        set_err(c, "MPP_ENC_SET_CFG failed");
        mpp_destroy(c->ctx);
        teardown_drm_buffers(c);
        free(c);
        return NULL;
    }

    /* post-init: SEI, header mode, drop config */
    config_sei_header_drop(c);

    /* smart GOP reference frames */
    config_gop_ref(c);

    /* non-blocking poll for epoll compatibility */
    c->poll_type = MPP_POLL_NON_BLOCK;
    ret = c->mpi->control(c->ctx, MPP_SET_OUTPUT_TIMEOUT,
                          (MppParam)&c->poll_type);
    if (ret != MPP_OK) {
        set_err(c, "set non-block timeout failed");
        mpp_destroy(c->ctx);
        teardown_drm_buffers(c);
        free(c);
        return NULL;
    }

    /* get extradata (SPS/PPS) */
    {
        MppPacket pkt = NULL;
        ret = c->mpi->control(c->ctx, MPP_ENC_GET_EXTRA_INFO, &pkt);
        if (ret == MPP_OK && pkt) {
            void *ptr = mpp_packet_get_data(pkt);
            size_t len = mpp_packet_get_length(pkt);
            if (ptr && len > 0) {
                c->extradata = malloc(len);
                if (c->extradata) {
                    memcpy(c->extradata, ptr, len);
                    c->extradata_len = len;
                }
            }
            mpp_packet_deinit(&pkt);
        }
    }

    c->inited = 1;
    return c;
}

/* ── destroy ──────────────────────────────────────────────────────────── */

void
ml_codec_destroy(ml_codec_t *c)
{
    if (!c) return;

    if (c->ctx) {
        if (c->inited) {
            MppFrame eos = NULL;
            mpp_frame_init(&eos);
            if (eos) {
                mpp_frame_set_eos(eos, 1);
                c->mpi->put_frame(c->ctx, eos);
                mpp_frame_deinit(&eos);
            }
        }
        mpp_destroy(c->ctx);
    }

    teardown_drm_buffers(c);
    free(c->extradata);
    free(c->pkt_data);
    pthread_mutex_destroy(&c->lock);
    free(c);
}

/* ── send frame ───────────────────────────────────────────────────────── */

/* copy NV12 planes with row-by-row respect for source/dest stride differences.
 * Returns 0 on success, -1 if source data is undersized. */
static int copy_nv12_to_mpp(void *dst_base,
                             const ml_frame_t *frame,
                             int dst_hor_stride, int dst_ver_stride)
{
    const uint8_t *src;
    uint8_t *dst = (uint8_t *)dst_base;
    uint32_t src_stride = frame->stride[0];
    uint32_t h = frame->height;
    uint32_t i;

    if (!src_stride) src_stride = frame->width;

    /* validate: source must contain at least width*h luma data */
    if (frame->size[0] < (size_t)frame->width * h) return -1;
    if (frame->data[1] && frame->size[1] < (size_t)frame->width * h / 2) return -1;

    /* Y plane — row-by-row when strides differ */
    src = (const uint8_t *)frame->data[0];
    for (i = 0; i < h; i++) {
        memcpy(dst, src, frame->width);
        dst += dst_hor_stride;
        src += src_stride;
    }

    /* UV plane — height/2 rows, width bytes per row (NV12 UV interleaved) */
    if (frame->data[1]) {
        src = (const uint8_t *)frame->data[1];
        src_stride = frame->stride[1];
        if (!src_stride) src_stride = frame->width;

        for (i = 0; i < h / 2; i++) {
            memcpy(dst, src, frame->width);
            dst += dst_hor_stride;
            src += src_stride;
        }
    }

    return 0;
}

/* ── send frame ───────────────────────────────────────────────────────── */

int
ml_codec_send_frame(ml_codec_t *c, ml_frame_t *frame)
{
    MppFrame mf = NULL;
    MppBuffer buf = NULL;
    MppRet ret;

    if (!c || !c->inited || !frame) return -1;

    /* MPP encoder requires NV12 semi-planar format */
    if (frame->format != ML_PIXFMT_NV12) { set_err(c, "send_frame: format must be NV12"); return -1; }
    if (!frame->data[0] || frame->width == 0 || frame->height == 0) { set_err(c, "send_frame: invalid frame"); return -1; }

    pthread_mutex_lock(&c->lock);

    ret = mpp_frame_init(&mf);
    if (ret != MPP_OK) {
        pthread_mutex_unlock(&c->lock);
        return -1;
    }

    mpp_frame_set_width(mf, frame->width);
    mpp_frame_set_height(mf, frame->height);
    mpp_frame_set_hor_stride(mf, c->hor_stride);
    mpp_frame_set_ver_stride(mf, c->ver_stride);
    mpp_frame_set_fmt(mf, MPP_FMT_YUV420SP);
    mpp_frame_set_eos(mf, 0);

    /* try DMA-BUF zero-copy path */
    if ((frame->flags & ML_FRAME_FLAG_DMABUF) && frame->fd[0] >= 0) {
        ret = mpp_buffer_get_fd(&buf, frame->fd[0]);
        if (ret == MPP_OK && buf) {
            mpp_frame_set_buffer(mf, buf);
        }
    }

    /* use pre-allocated DRM buffer if DMA-BUF import failed */
    if (!buf && c->frame_buf) {
        void *dst = mpp_buffer_get_ptr(c->frame_buf);
        if (copy_nv12_to_mpp(dst, frame, c->hor_stride, c->ver_stride) == 0) {
            mpp_frame_set_buffer(mf, c->frame_buf);
            buf = c->frame_buf;
        }
    }

    /* fallback: allocate new MPP buffer */
    if (!buf) {
        size_t total = (size_t)c->hor_stride * c->ver_stride * 3 / 2;

        ret = mpp_buffer_get(NULL, &buf, total);
        if (ret != MPP_OK || !buf) {
            mpp_frame_deinit(&mf);
            pthread_mutex_unlock(&c->lock);
            return -1;
        }

        void *dst = mpp_buffer_get_ptr(buf);
        if (copy_nv12_to_mpp(dst, frame, c->hor_stride, c->ver_stride) < 0) {
            mpp_buffer_put(buf);
            mpp_frame_deinit(&mf);
            pthread_mutex_unlock(&c->lock);
            set_err(c, "send_frame: source data underflow");
            return -1;
        }

        mpp_frame_set_buffer(mf, buf);
    }

    mpp_frame_set_pts(mf, frame->timestamp / 1000);

    ret = c->mpi->put_frame(c->ctx, mf);
    mpp_frame_deinit(&mf);

    pthread_mutex_unlock(&c->lock);
    return (ret == MPP_OK) ? 0 : -1;
}

/* ── receive packet ───────────────────────────────────────────────────── */

int
ml_codec_receive_packet(ml_codec_t *c, ml_codec_pkt_t *pkt)
{
    MppPacket mp = NULL;
    MppRet ret;

    if (!c || !c->inited || !pkt) return -1;

    memset(pkt, 0, sizeof(*pkt));

    pthread_mutex_lock(&c->lock);

    ret = c->mpi->get_packet(c->ctx, &mp);
    if (ret == MPP_ERR_TIMEOUT || !mp) {
        pthread_mutex_unlock(&c->lock);
        return ML_EAGAIN;
    }
    if (ret != MPP_OK) {
        pthread_mutex_unlock(&c->lock);
        return -1;
    }

    /* copy data out of MPP packet before deinit (pkt->data must outlive mp) */
    {
        size_t len = mpp_packet_get_length(mp);
        if (len > c->pkt_data_cap) {
            uint8_t *tmp = realloc(c->pkt_data, len);
            if (!tmp) {
                mpp_packet_deinit(&mp);
                pthread_mutex_unlock(&c->lock);
                return -1;
            }
            c->pkt_data     = tmp;
            c->pkt_data_cap = len;
        }
        memcpy(c->pkt_data, mpp_packet_get_data(mp), len);
        pkt->data = c->pkt_data;
        pkt->size = (int)len;
    }

    pkt->pts       = mpp_packet_get_pts(mp);
    pkt->dts       = mpp_packet_get_dts(mp);
    pkt->keyframe  = mpp_packet_is_key(mp) ? 1 : 0;

    /* determine frame type and optional metadata */
    if (mpp_packet_has_meta(mp)) {
        MppMeta meta = mpp_packet_get_meta(mp);
        RK_S32 frame_type = 1;
        if (meta) {
            mpp_meta_get_s32(meta, KEY_OUTPUT_FRAME_TYPE, &frame_type);
        }
        pkt->frame_type = (frame_type == 0) ? 0 : 1;
    } else {
        pkt->frame_type = 1;
    }

    mpp_packet_deinit(&mp);

    pthread_mutex_unlock(&c->lock);
    return 0;
}

/* ── flush ────────────────────────────────────────────────────────────── */

int
ml_codec_flush(ml_codec_t *c)
{
    ml_codec_pkt_t pkt;
    int ret, total = 0;

    if (!c || !c->inited) return -1;

    {
        MppFrame eos = NULL;
        mpp_frame_init(&eos);
        if (eos) {
            mpp_frame_set_eos(eos, 1);
            c->mpi->put_frame(c->ctx, eos);
            mpp_frame_deinit(&eos);
        }
    }

    while ((ret = ml_codec_receive_packet(c, &pkt)) == 0) {
        total++;
    }

    if (ret == ML_EAGAIN) return total;
    return ret;
}

/* ── extradata ────────────────────────────────────────────────────────── */

int
ml_codec_get_extradata(ml_codec_t *c, uint8_t **data, size_t *size)
{
    if (!c || !c->inited || !data || !size) return -1;
    if (!c->extradata || c->extradata_len == 0) return -1;

    *data = c->extradata;
    *size = c->extradata_len;
    return 0;
}

/* ── query ────────────────────────────────────────────────────────────── */

const char *
ml_codec_error(ml_codec_t *c)
{
    if (!c || !c->err_msg[0]) return NULL;
    return c->err_msg;
}

int
ml_codec_backend_available(ml_codec_backend_t backend)
{
    switch (backend) {
    case ML_CODEC_BACKEND_MPP:
        return (access("/dev/mpp_service", F_OK) == 0) ? 1 : 0;
    case ML_CODEC_BACKEND_VAAPI:
        return (access("/dev/dri/renderD128", F_OK) == 0) ? 1 : 0;
    case ML_CODEC_BACKEND_NVCODEC:
        return (access("/dev/nvhost-msenc", F_OK) == 0) ? 1 : 0;
    case ML_CODEC_BACKEND_SOFTWARE:
        return 1;
    default:
        return 0;
    }
}
