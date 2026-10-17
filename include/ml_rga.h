/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_rga — Rockchip RGA 2D accelerator wrapper.
 *
 * Provides zero-copy image resize, color-space conversion (CSC),
 * rotation, and flip via the hardware RGA engine.  On non-Rockchip
 * platforms, all operations return ML_ENOSYS.
 *
 * Lifecycle:
 *   create → execute* (or convenience wrappers) → destroy
 */

#ifndef __ML_RGA_H__
#define __ML_RGA_H__

#include <stdint.h>
#include <stddef.h>

/* ── forward declarations ─────────────────────────────────────────────── */

typedef struct ml_pool_s ml_pool_t;
typedef struct ml_rga_s  ml_rga_t;

/* ── enums ────────────────────────────────────────────────────────────── */

typedef enum {
    ML_RGA_FMT_BGR888 = 0,
    ML_RGA_FMT_RGB888,
    ML_RGA_FMT_NV12,     /* Y + UV two-plane (MPP native) */
    ML_RGA_FMT_NV21,     /* Y + VU two-plane              */
    ML_RGA_FMT_RGBA8888,
    ML_RGA_FMT_BGRA8888,
} ml_rga_format_t;

typedef enum {
    ML_RGA_ROTATE_0   = 0,
    ML_RGA_ROTATE_90  = 1,
    ML_RGA_ROTATE_180 = 2,
    ML_RGA_ROTATE_270 = 3,
} ml_rga_rotate_t;

typedef enum {
    ML_RGA_FLIP_NONE       = 0,
    ML_RGA_FLIP_HORIZONTAL = 1,
    ML_RGA_FLIP_VERTICAL   = 2,
    ML_RGA_FLIP_BOTH       = 3,
} ml_rga_flip_t;

/* ── RGA operation descriptor ─────────────────────────────────────────── */

typedef struct {
    /* source */
    int             src_fd;       /* DMA-BUF fd, or -1 for user-space data */
    const void     *src_data;     /* user-space pointer if src_fd == -1   */
    int             src_width;
    int             src_height;
    int             src_stride;   /* pixel stride (width * bpp/8)         */
    ml_rga_format_t src_format;

    /* source ROI (region of interest) — 0 means full image */
    int             src_x, src_y;
    int             src_w, src_h; /* 0 = use src_width/height            */

    /* destination */
    int             dst_fd;       /* DMA-BUF fd, or -1                    */
    void           *dst_data;     /* user-space pointer if dst_fd == -1   */
    int             dst_width;
    int             dst_height;
    int             dst_stride;
    ml_rga_format_t dst_format;

    /* destination ROI */
    int             dst_x, dst_y;
    int             dst_w, dst_h; /* 0 = use dst_width/height            */

    /* transform */
    ml_rga_rotate_t rotate;
    ml_rga_flip_t   flip;

    /* misc */
    int             dither;       /* enable dither for bit-depth reduction */
} ml_rga_op_t;

#define ML_RGA_OP_DEFAULT {  \
    -1, NULL, 0, 0, 0, 0,   \
    0, 0, 0, 0,             \
    -1, NULL, 0, 0, 0, 0,   \
    0, 0, 0, 0,             \
    ML_RGA_ROTATE_0,         \
    ML_RGA_FLIP_NONE,        \
    0                        \
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

/* Create RGA context.  pool is used for temporary allocations.
 * Returns NULL if RGA hardware is not available. */
ml_rga_t *ml_rga_create(ml_pool_t *pool);

/* Destroy RGA context. Safe on NULL. */
void      ml_rga_destroy(ml_rga_t *rga);

/* ── core operation ───────────────────────────────────────────────────── */

/* Execute one RGA operation (blit + scale + CSC + rotate + flip).
 * Synchronous: blocks until the operation completes.
 * DMA-BUF path: if both src_fd and dst_fd are valid, uses zero-copy.
 * Returns 0 on success, -1 on error. */
int  ml_rga_execute(ml_rga_t *rga, const ml_rga_op_t *op);

/* ── convenience wrappers ─────────────────────────────────────────────── */

/* Resize image (no format change).
 *   rga:     RGA context
 *   src:     source data pointer
 *   src_w/h: source dimensions
 *   dst:     destination buffer (pre-allocated)
 *   dst_w/h: destination dimensions
 *   fmt:     pixel format (must match src & dst)
 * Returns 0 on success, -1 on error. */
int  ml_rga_resize(ml_rga_t *rga,
                   const void *src, int src_w, int src_h,
                   void *dst, int dst_w, int dst_h,
                   ml_rga_format_t fmt);

/* Color-space conversion (no resize).
 *   rga:       RGA context
 *   src:       source data
 *   w, h:      dimensions (same for src & dst)
 *   src_fmt:   input format
 *   dst:       destination buffer (pre-allocated)
 *   dst_fmt:   output format
 * Returns 0 on success, -1 on error. */
int  ml_rga_csc(ml_rga_t *rga,
                const void *src, int w, int h, ml_rga_format_t src_fmt,
                void *dst, ml_rga_format_t dst_fmt);

/* ── query ────────────────────────────────────────────────────────────── */

/* Last error message. Returns NULL if no error. */
const char *ml_rga_error(ml_rga_t *rga);

/* Return 1 if RGA hardware is available on this system. */
int  ml_rga_available(void);

#endif /* __ML_RGA_H__ */
