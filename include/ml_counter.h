/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_counter — Hysteresis counter for domain event detection.
 *
 * Counts upward when a float signal crosses above a threshold, downward
 * when it drops below (threshold - hysteresis).  The hysteresis band
 * prevents chatter around the threshold.
 */

#ifndef __ML_COUNTER_H__
#define __ML_COUNTER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t count;
    float   threshold;
    float   hysteresis;
    int     armed;       /* 1 = ready to count next upward crossing */
} ml_counter_t;

void    ml_counter_init(ml_counter_t *c, float threshold, float hysteresis);
int     ml_counter_update(ml_counter_t *c, float value);
int32_t ml_counter_get(const ml_counter_t *c);
void    ml_counter_reset(ml_counter_t *c);

#ifdef __cplusplus
}
#endif

#endif /* __ML_COUNTER_H__ */
