/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_range_finder — Peak/valley detection in a float signal.
 *
 * Detects local maxima (peaks) and minima (valleys) with configurable
 * minimum threshold and minimum distance between events.
 *
 * Returns: 1 = peak detected, -1 = valley detected, 0 = no event.
 */

#ifndef __ML_RANGE_FINDER_H__
#define __ML_RANGE_FINDER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    min_threshold;   /* minimum vertical distance to count as peak/valley */
    uint32_t min_distance;    /* minimum samples between events */
    uint32_t samples_since;   /* samples since last event */
    float    last_peak_val;
    float    last_valley_val;
    int      direction;       /* 1 = looking for peak, -1 = looking for valley */
    float    prev_value;
    int      initialized;
} ml_range_finder_t;

void ml_range_finder_init(ml_range_finder_t *rf, float min_threshold,
                          uint32_t min_distance);
int  ml_range_finder_update(ml_range_finder_t *rf, float value);
void ml_range_finder_reset(ml_range_finder_t *rf);

#ifdef __cplusplus
}
#endif

#endif /* __ML_RANGE_FINDER_H__ */
