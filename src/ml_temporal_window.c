/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_temporal_window.h"
#include "ml_pool.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

void
ml_temporal_window_init(ml_temporal_window_t *w, uint32_t capacity, ml_pool_t *pool)
{
    if (!w || !pool) return;
    w->buf       = NULL;
    w->capacity  = 0;
    w->count     = 0;
    w->write_pos = 0;
    w->pool      = pool;
    if (capacity == 0) return;
    w->buf = (float *)pool->f_alloc(pool, capacity * sizeof(float));
    if (!w->buf) return;
    memset(w->buf, 0, capacity * sizeof(float));
    w->capacity = capacity;
}

void
ml_temporal_window_push(ml_temporal_window_t *w, float value)
{
    if (!w || !w->buf || w->capacity == 0) return;
    w->buf[w->write_pos] = value;
    w->write_pos = (w->write_pos + 1) % w->capacity;
    if (w->count < w->capacity)
        w->count++;
}

void
ml_temporal_window_free(ml_temporal_window_t *w)
{
    if (!w || !w->buf) return;
    w->pool->f_free(w->pool, w->buf);
    w->buf      = NULL;
    w->capacity = 0;
    w->count    = 0;
}

uint32_t
ml_temporal_window_count(const ml_temporal_window_t *w)
{
    return w ? w->count : 0;
}

int
ml_temporal_window_ready(const ml_temporal_window_t *w)
{
    return (w && w->count >= w->capacity) ? 1 : 0;
}

float
ml_temporal_window_sum(const ml_temporal_window_t *w)
{
    float s = 0.0f;
    uint32_t i;
    if (!w || !w->buf) return 0.0f;
    for (i = 0; i < w->count; i++)
        s += w->buf[i];
    return s;
}

float
ml_temporal_window_mean(const ml_temporal_window_t *w)
{
    if (!w || w->count == 0) return 0.0f;
    return ml_temporal_window_sum(w) / (float)w->count;
}

float
ml_temporal_window_min(const ml_temporal_window_t *w)
{
    float m = FLT_MAX;
    uint32_t i;
    if (!w || !w->buf || w->count == 0) return 0.0f;
    for (i = 0; i < w->count; i++)
        if (w->buf[i] < m) m = w->buf[i];
    return m;
}

float
ml_temporal_window_max(const ml_temporal_window_t *w)
{
    float m = -FLT_MAX;
    uint32_t i;
    if (!w || !w->buf || w->count == 0) return 0.0f;
    for (i = 0; i < w->count; i++)
        if (w->buf[i] > m) m = w->buf[i];
    return m;
}

float
ml_temporal_window_get(const ml_temporal_window_t *w, uint32_t index)
{
    int pos;
    if (!w || !w->buf || index >= w->count) return 0.0f;
    pos = (int)(w->write_pos + w->capacity - w->count + index) % (int)w->capacity;
    return w->buf[pos];
}
