/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_range_finder.h"

void
ml_range_finder_init(ml_range_finder_t *rf, float min_threshold,
                     uint32_t min_distance)
{
    if (!rf) return;
    rf->min_threshold  = min_threshold;
    rf->min_distance   = min_distance > 0 ? min_distance : 1;
    rf->samples_since  = rf->min_distance; /* ready immediately */
    rf->last_peak_val  = 0.0f;
    rf->last_valley_val = 0.0f;
    rf->direction      = 1;  /* start looking for a peak */
    rf->prev_value     = 0.0f;
    rf->initialized    = 0;
}

int
ml_range_finder_update(ml_range_finder_t *rf, float value)
{
    if (!rf) return 0;

    rf->samples_since++;

    if (!rf->initialized) {
        rf->prev_value  = value;
        rf->initialized = 1;
        return 0;
    }

    /* direction sign change → potential peak/valley */
    if (rf->direction > 0 && value < rf->prev_value) {
        /* was going up, now going down → peak candidate */
        if (rf->samples_since >= rf->min_distance &&
            rf->prev_value - rf->last_valley_val >= rf->min_threshold) {
            rf->direction     = -1;
            rf->last_peak_val = rf->prev_value;
            rf->samples_since = 0;
            rf->prev_value    = value;
            return 1; /* peak */
        }
    } else if (rf->direction < 0 && value > rf->prev_value) {
        /* was going down, now going up → valley candidate */
        if (rf->samples_since >= rf->min_distance &&
            rf->last_peak_val - rf->prev_value >= rf->min_threshold) {
            rf->direction       = 1;
            rf->last_valley_val = rf->prev_value;
            rf->samples_since   = 0;
            rf->prev_value      = value;
            return -1; /* valley */
        }
    }

    /* update direction based on trend */
    if (value > rf->prev_value)
        rf->direction = 1;
    else if (value < rf->prev_value)
        rf->direction = -1;

    rf->prev_value = value;
    return 0;
}

void
ml_range_finder_reset(ml_range_finder_t *rf)
{
    if (!rf) return;
    rf->samples_since   = rf->min_distance;
    rf->last_peak_val   = 0.0f;
    rf->last_valley_val = 0.0f;
    rf->direction       = 1;
    rf->prev_value      = 0.0f;
    rf->initialized     = 0;
}
