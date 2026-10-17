/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_queue — Thread-safe bounded queue for pipeline stage communication.
 *
 * Fixed-capacity circular buffer with mutex + condition variable.
 * Supports blocking (with timeout), non-blocking try, and infinite-wait
 * push/pop operations.  Stores opaque void* pointers — ownership of
 * pointed-to data stays with the caller.
 *
 * Thread safety: FULLY thread-safe.  All push/pop/query operations use
 * internal mutex protection.  ml_queue_interrupt can be called from any
 * thread to break blocked waits.  ml_queue_destroy is NOT safe to call
 * while other threads are still using the queue.
 */

#ifndef __ML_QUEUE_H__
#define __ML_QUEUE_H__

#include <stdbool.h>
#include "ml_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ml_queue_s ml_queue_t;
typedef struct ml_pool_s  ml_pool_t;

/* ── lifecycle ───────────────────────────────────────────────────── */

/* Create a bounded queue with the given capacity (max items).
 * capacity must be >= 1. */
ml_queue_t *ml_queue_create(int capacity, ml_pool_t *pool);
void        ml_queue_destroy(ml_queue_t *q);

/* ── blocking push / pop ─────────────────────────────────────────── */

/* Push an item.  timeout_ms:
 *   < 0  → wait forever
 *   = 0  → try once, return immediately (same as ml_queue_try_push)
 *   > 0  → wait up to timeout_ms milliseconds
 * Returns 0 on success, ML_EAGAIN on timeout, ML_ERROR on failure. */
int ml_queue_push(ml_queue_t *q, void *item, int timeout_ms);

/* Pop an item.  Same timeout semantics as push.
 * Returns 0 on success (*item is set), ML_EAGAIN on timeout. */
int ml_queue_pop(ml_queue_t *q, void **item, int timeout_ms);

/* ── non-blocking ────────────────────────────────────────────────── */

static inline int ml_queue_try_push(ml_queue_t *q, void *item) {
    return ml_queue_push(q, item, 0);
}
static inline int ml_queue_try_pop(ml_queue_t *q, void **item) {
    return ml_queue_pop(q, item, 0);
}

/* ── query ───────────────────────────────────────────────────────── */

int  ml_queue_count(const ml_queue_t *q);
int  ml_queue_capacity(const ml_queue_t *q);
bool ml_queue_empty(const ml_queue_t *q);
bool ml_queue_full(const ml_queue_t *q);

void ml_queue_interrupt(ml_queue_t *q);

/* ── graceful shutdown ────────────────────────────────────────────── */

/* Signal that no more items will be pushed.  Unlike interrupt(),
 * close() allows the queue to be drained first — pop() will keep
 * returning items until the queue is empty, then return ML_ERROR.
 * push() to a closed queue returns ML_ERROR immediately. */
void ml_queue_close(ml_queue_t *q);
int  ml_queue_is_closed(const ml_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* __ML_QUEUE_H__ */
