/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_rga.c — Rockchip RGA 2D accelerator implementation.
 *
 * Uses librga.so to offload resize, CSC, rotation, and flip to the
 * RGA hardware engine.  Prefers DMA-BUF zero-copy when both src and
 * dst are DMA-BUF backed; otherwise falls back to virtual-address mode.
 *
 * Compiles only on Linux with librga installed.
 */

#include "ml_rga.h"
#include "ml_pool.h"

#include <rga/RgaApi.h>
#include <rga/RgaUtils.h>
#include <rga/rga.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── internal struct ──────────────────────────────────────────────────── */

struct ml_rga_s {
    ml_pool_t *pool;
    int        inited;
    int        version;          /* RGA driver version    */
    char       err_msg[256];
};

/* ── helpers ──────────────────────────────────────────────────────────── */

static void set_err(ml_rga_t *rga, const char *msg)
{
    if (rga && msg) {
        strncpy(rga->err_msg, msg, sizeof(rga->err_msg) - 1);
        rga->err_msg[sizeof(rga->err_msg) - 1] = '\0';
    }
}

/* convert ml_rga_format_t to RGA format constant */
static int to_rga_format(ml_rga_format_t fmt)
{
    switch (fmt) {
    case ML_RGA_FMT_BGR888:   return RK_FORMAT_BGR_888;
    case ML_RGA_FMT_RGB888:   return RK_FORMAT_RGB_888;
    case ML_RGA_FMT_NV12:     return RK_FORMAT_YCbCr_420_SP;
    case ML_RGA_FMT_NV21:     return RK_FORMAT_YCrCb_420_SP;
    case ML_RGA_FMT_RGBA8888: return RK_FORMAT_RGBA_8888;
    case ML_RGA_FMT_BGRA8888: return RK_FORMAT_BGRA_8888;
    default:                  return -1;
    }
}

static int to_rga_rotate(ml_rga_rotate_t r)
{
    switch (r) {
    case ML_RGA_ROTATE_90:  return HAL_TRANSFORM_ROT_90;
    case ML_RGA_ROTATE_180: return HAL_TRANSFORM_ROT_180;
    case ML_RGA_ROTATE_270: return HAL_TRANSFORM_ROT_270;
    default:                return 0;
    }
}

static int to_rga_flip(ml_rga_flip_t f)
{
    switch (f) {
    case ML_RGA_FLIP_HORIZONTAL: return HAL_TRANSFORM_FLIP_H;
    case ML_RGA_FLIP_VERTICAL:   return HAL_TRANSFORM_FLIP_V;
    case ML_RGA_FLIP_BOTH:       return HAL_TRANSFORM_FLIP_H_V;
    default:                     return 0;
    }
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

ml_rga_t *
ml_rga_create(ml_pool_t *pool)
{
    ml_rga_t *rga;

    if (!ml_rga_available()) return NULL;

    rga = calloc(1, sizeof(*rga));
    if (!rga) return NULL;

    rga->pool = pool;

    if (c_RkRgaInit() < 0) {
        set_err(rga, "RGA init failed");
        free(rga);
        return NULL;
    }

    rga->inited = 1;

    /* read driver version */
    {
        drv_version_t ver;
        if (c_RkRgaGetVersion(&ver) == 0)
            rga->version = ver.major * 10000 + ver.minor * 100 + ver.revision;
    }

    return rga;
}

void
ml_rga_destroy(ml_rga_t *rga)
{
    if (!rga) return;
    if (rga->inited) c_RkRgaDeInit();
    free(rga);
}

/* ── execute ──────────────────────────────────────────────────────────── */

int
ml_rga_execute(ml_rga_t *rga, const ml_rga_op_t *op)
{
    rga_info_t src, dst;
    int src_fmt, dst_fmt;
    int src_w, src_h, dst_w, dst_h;
    int ret;

    if (!rga || !rga->inited || !op) return -1;

    src_fmt = to_rga_format(op->src_format);
    dst_fmt = to_rga_format(op->dst_format);
    if (src_fmt < 0 || dst_fmt < 0) {
        set_err(rga, "unsupported format");
        return -1;
    }

    src_w = op->src_w ? op->src_w : op->src_width;
    src_h = op->src_h ? op->src_h : op->src_height;
    dst_w = op->dst_w ? op->dst_w : op->dst_width;
    dst_h = op->dst_h ? op->dst_h : op->dst_height;

    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    /* source */
    src.fd = op->src_fd;
    if (op->src_fd < 0 && op->src_data) {
        src.virAddr = (void *)op->src_data;
        src.mmuFlag = 0; /* virtual address mode */
    } else {
        src.mmuFlag = 1; /* DMA-BUF mode */
    }
    rga_set_rect(&src.rect, op->src_x, op->src_y,
                 src_w, src_h,
                 op->src_stride ? op->src_stride : src_w,
                 src_h, src_fmt);

    /* destination */
    dst.fd = op->dst_fd;
    if (op->dst_fd < 0 && op->dst_data) {
        dst.virAddr = op->dst_data;
        dst.mmuFlag = 0;
    } else {
        dst.mmuFlag = 1;
    }
    rga_set_rect(&dst.rect, op->dst_x, op->dst_y,
                 dst_w, dst_h,
                 op->dst_stride ? op->dst_stride : dst_w,
                 dst_h, dst_fmt);

    /* transform */
    src.rotation = to_rga_rotate(op->rotate) | to_rga_flip(op->flip);

    /* dither */
    if (op->dither)
        src.dither.enable = 1;

    ret = c_RkRgaBlit(&src, &dst, NULL);
    if (ret < 0) {
        set_err(rga, "RGA blit failed");
        return -1;
    }

    return 0;
}

/* ── convenience wrappers ─────────────────────────────────────────────── */

int
ml_rga_resize(ml_rga_t *rga,
              const void *src, int src_w, int src_h,
              void *dst, int dst_w, int dst_h,
              ml_rga_format_t fmt)
{
    ml_rga_op_t op = ML_RGA_OP_DEFAULT;

    op.src_data   = src;
    op.src_width  = src_w;
    op.src_height = src_h;
    op.src_format = fmt;
    op.src_w      = src_w;
    op.src_h      = src_h;

    op.dst_data   = dst;
    op.dst_width  = dst_w;
    op.dst_height = dst_h;
    op.dst_format = fmt;
    op.dst_w      = dst_w;
    op.dst_h      = dst_h;

    return ml_rga_execute(rga, &op);
}

int
ml_rga_csc(ml_rga_t *rga,
           const void *src, int w, int h, ml_rga_format_t src_fmt,
           void *dst, ml_rga_format_t dst_fmt)
{
    ml_rga_op_t op = ML_RGA_OP_DEFAULT;

    op.src_data   = src;
    op.src_width  = w;
    op.src_height = h;
    op.src_format = src_fmt;
    op.src_w      = w;
    op.src_h      = h;

    op.dst_data   = dst;
    op.dst_width  = w;
    op.dst_height = h;
    op.dst_format = dst_fmt;
    op.dst_w      = w;
    op.dst_h      = h;

    return ml_rga_execute(rga, &op);
}

/* ── query ────────────────────────────────────────────────────────────── */

const char *
ml_rga_error(ml_rga_t *rga)
{
    if (!rga || !rga->err_msg[0]) return NULL;
    return rga->err_msg;
}

int
ml_rga_available(void)
{
    /* check if RGA device node exists */
    return (access("/dev/rga", F_OK) == 0) ? 1 : 0;
}
