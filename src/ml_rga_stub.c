/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_rga_stub.c — macOS / non-Rockchip stub.
 */

#include "ml_rga.h"
#include "ml_pool.h"
#include <stdlib.h>

struct ml_rga_s {
    ml_pool_t *pool;
    char       err_msg[256];
};

ml_rga_t *ml_rga_create(ml_pool_t *pool)
{
    if (!pool) return NULL;
    ml_rga_t *rga = pool->f_alloc(pool, sizeof(*rga));
    if (!rga) return NULL;
    memset(rga, 0, sizeof(*rga));
    rga->pool = pool;
    snprintf(rga->err_msg, sizeof(rga->err_msg),
             "RGA not available on this platform");
    /* return non-NULL so API doesn't crash, but all ops fail */
    return rga;
}

void ml_rga_destroy(ml_rga_t *rga) {
    if (rga && rga->pool) rga->pool->f_free(rga->pool, rga);
}

int ml_rga_resize(ml_rga_t *rga,
                  const void *s, int sw, int sh,
                  void *d, int dw, int dh, ml_rga_format_t f)
{ (void)rga; (void)s; (void)sw; (void)sh; (void)d; (void)dw; (void)dh; (void)f; return -1; }

int ml_rga_csc(ml_rga_t *rga,
               const void *s, int w, int h, ml_rga_format_t sf,
               void *d, ml_rga_format_t df)
{ (void)rga; (void)s; (void)w; (void)h; (void)sf; (void)d; (void)df; return -1; }

int ml_rga_execute(ml_rga_t *rga, const ml_rga_op_t *op)
{ (void)rga; (void)op; return -1; }

const char *ml_rga_error(ml_rga_t *rga)
{
    if (!rga || !rga->err_msg[0]) return NULL;
    return rga->err_msg;
}

int ml_rga_available(void) { return 0; }
