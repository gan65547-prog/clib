/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_codec_stub.c — macOS / non-MPP stub implementation.
 *
 * All operations return error codes.  Compile this instead of
 * ml_codec_mpp.c on platforms without Rockchip MPP.
 */

#include "ml_codec.h"
#include "ml_frame.h"
#include "ml_pool.h"

#include <stdlib.h>

struct ml_codec_s {
    ml_codec_cfg_t  cfg;
    ml_pool_t      *pool;
    char            err_msg[256];
    int             inited;
};

/* ── lifecycle ────────────────────────────────────────────────────────── */

ml_codec_t *
ml_codec_create(const ml_codec_cfg_t *cfg, ml_pool_t *pool)
{
    if (!pool) return NULL;
    ml_codec_t *c = pool->f_alloc(pool, sizeof(*c));
    if (!c) return NULL;
    memset(c, 0, sizeof(*c));

    if (cfg)
        c->cfg = *cfg;
    else
        c->cfg = (ml_codec_cfg_t)ML_CODEC_CFG_DEFAULT;

    c->pool = pool;
    snprintf(c->err_msg, sizeof(c->err_msg),
             "MPP encoder not available on this platform");
    return c;
}

void
ml_codec_destroy(ml_codec_t *c)
{
    if (c && c->pool) c->pool->f_free(c->pool, c);
}

/* ── encoding (all fail) ──────────────────────────────────────────────── */

int
ml_codec_send_frame(ml_codec_t *c, ml_frame_t *frame)
{
    (void)c; (void)frame;
    return -1;
}

int
ml_codec_receive_packet(ml_codec_t *c, ml_codec_pkt_t *pkt)
{
    (void)c; (void)pkt;
    return -1;
}

int
ml_codec_flush(ml_codec_t *c)
{
    if (!c) return -1;
    return 0;
}

int
ml_codec_get_extradata(ml_codec_t *c, uint8_t **data, size_t *size)
{
    (void)c; (void)data; (void)size;
    return -1;
}

/* ── query ────────────────────────────────────────────────────────────── */

const char *
ml_codec_error(ml_codec_t *c)
{
    if (!c) return NULL;
    return c->err_msg;
}

int
ml_codec_backend_available(ml_codec_backend_t backend)
{
    return (backend == ML_CODEC_BACKEND_SOFTWARE) ? 1 : 0;
}
