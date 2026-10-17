/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_debounce — Binary signal debouncing.
 *
 * A binary signal must stay stable for `threshold` consecutive samples
 * before the output changes.  Filters out glitches and brief dropouts.
 */

#ifndef __ML_DEBOUNCE_H__
#define __ML_DEBOUNCE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t stable_count;
    uint32_t threshold;
    int      output;        /* current debounced output (0 or 1) */
    int      pending;       /* pending state if different from output */
} ml_debounce_t;

void ml_debounce_init(ml_debounce_t *d, uint32_t threshold);
int  ml_debounce_update(ml_debounce_t *d, int signal);
int  ml_debounce_get(const ml_debounce_t *d);
void ml_debounce_reset(ml_debounce_t *d);

#ifdef __cplusplus
}
#endif

#endif /* __ML_DEBOUNCE_H__ */
