/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#ifndef __ML_THREAD_POOL_H__
#define __ML_THREAD_POOL_H__

#include "ml_types.h"

typedef struct ml_pool_s        ml_pool_t;
typedef struct ml_thread_pool_s ml_thread_pool_t;

/*
 * Create a thread pool with nthreads worker threads.
 * max_queue: max pending tasks (0 = unbounded).
 * Returns NULL on failure.
 */
ml_thread_pool_t *ml_thread_pool_create(ml_pool_t *pool, ml_int_t nthreads,
                                         ml_int_t max_queue);

/* Destroy the pool. Waits for running tasks, discards queued tasks. */
void ml_thread_pool_destroy(ml_thread_pool_t *tp);

/* Submit a task. Returns ML_OK on success, ML_ERROR if queue is full. */
ml_int_t ml_thread_pool_submit(ml_thread_pool_t *tp, void (*func)(void *), void *arg);

/* Block until all submitted tasks have completed. */
void ml_thread_pool_wait(ml_thread_pool_t *tp);

/* Pause task dispatch. Running tasks complete; queued tasks are held until resume. */
void ml_thread_pool_pause(ml_thread_pool_t *tp);

/* Resume dispatch after pause. */
void ml_thread_pool_resume(ml_thread_pool_t *tp);

/* Return number of threads currently executing a task. */
ml_int_t ml_thread_pool_get_working_count(ml_thread_pool_t *tp);

#endif
