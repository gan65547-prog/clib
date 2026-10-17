/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_debounce.h"

void
ml_debounce_init(ml_debounce_t *d, uint32_t threshold)
{
    if (!d) return;
    d->stable_count = 0;
    d->threshold    = threshold > 0 ? threshold : 1;
    d->output       = 0;
    d->pending      = 0;
}

int
ml_debounce_update(ml_debounce_t *d, int signal)
{
    if (!d) return 0;

    if (signal == d->output) {
        d->stable_count = 0;
        d->pending      = d->output;
        return 0; /* no change */
    }

    if (signal != d->pending) {
        d->pending      = signal;
        d->stable_count = 1;
    } else {
        d->stable_count++;
    }

    if (d->stable_count >= d->threshold) {
        d->output       = d->pending;
        d->stable_count = 0;
        return 1; /* state changed */
    }

    return 0;
}

int
ml_debounce_get(const ml_debounce_t *d)
{
    return d ? d->output : 0;
}

void
ml_debounce_reset(ml_debounce_t *d)
{
    if (!d) return;
    d->stable_count = 0;
    d->output       = 0;
    d->pending      = 0;
}
