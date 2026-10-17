/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_counter.h"

void
ml_counter_init(ml_counter_t *c, float threshold, float hysteresis)
{
    if (!c) return;
    c->count     = 0;
    c->threshold = threshold;
    c->hysteresis = hysteresis;
    c->armed     = 1;
}

int
ml_counter_update(ml_counter_t *c, float value)
{
    if (!c) return 0;

    if (c->armed && value >= c->threshold) {
        c->count++;
        c->armed = 0;
        return 1;
    }

    if (!c->armed && value < c->threshold - c->hysteresis) {
        c->armed = 1;
    }

    return 0;
}

int32_t
ml_counter_get(const ml_counter_t *c)
{
    return c ? c->count : 0;
}

void
ml_counter_reset(ml_counter_t *c)
{
    if (!c) return;
    c->count = 0;
    c->armed = 1;
}
