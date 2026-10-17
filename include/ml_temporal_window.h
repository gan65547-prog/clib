/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_temporal_window — Fixed-size sliding window for float values.
 *
 * Accumulates the last N float samples.  Useful for moving averages,
 * trend detection, and temporal pattern matching in domain plugins.
 */

#ifndef __ML_TEMPORAL_WINDOW_H__
#define __ML_TEMPORAL_WINDOW_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   *buf;
    uint32_t capacity;
    uint32_t count;
    uint32_t write_pos;
} ml_temporal_window_t;

void ml_temporal_window_init(ml_temporal_window_t *w, uint32_t capacity);
void ml_temporal_window_push(ml_temporal_window_t *w, float value);
void ml_temporal_window_free(ml_temporal_window_t *w);

uint32_t ml_temporal_window_count(const ml_temporal_window_t *w);
int      ml_temporal_window_ready(const ml_temporal_window_t *w);

float    ml_temporal_window_sum(const ml_temporal_window_t *w);
float    ml_temporal_window_mean(const ml_temporal_window_t *w);
float    ml_temporal_window_min(const ml_temporal_window_t *w);
float    ml_temporal_window_max(const ml_temporal_window_t *w);
float    ml_temporal_window_get(const ml_temporal_window_t *w, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* __ML_TEMPORAL_WINDOW_H__ */
