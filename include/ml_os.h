/*
 * ml_os — OS-level helpers.
 *
 * Provides page size query and a millisecond monotonic clock (ml_uptime)
 * used by the timer wheel and event loop.
 */
#ifndef __ML_OS_H__
#define __ML_OS_H__

#include <unistd.h>
#include <string.h>
#include <time.h>
#include "ml_types.h"
#include "ml_timer.h"

typedef long ml_time_t;

/* Return the system page size. */
static inline ml_int_t ml_pagesize(void) {
    return (ml_int_t)sysconf(_SC_PAGESIZE);
}

/* Monotonic millisecond clock — immune to NTP / wall-clock adjustments. */
static inline ml_msec_t ml_uptime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ml_msec_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

#endif
