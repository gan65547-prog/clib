/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_pipeline_stage — Composable single-threaded pipeline stage.
 *
 * Each stage wraps a processing function in its own thread, with bounded
 * input and output queues providing natural backpressure.  Stages are
 * connected by sharing queues: the output queue of stage N is the input
 * queue of stage N+1 (set manually via ml_pipeline_stage_set_input /
 * ml_pipeline_stage_set_output after creation).
 *
 * When both input and output queues are provided externally (connected
 * mode), the stage does not own them and will not destroy them.
 */

#ifndef __ML_PIPELINE_STAGE_H__
#define __ML_PIPELINE_STAGE_H__

#include "ml_queue.h"

typedef struct ml_pipeline_stage_s ml_pipeline_stage_t;

/* Processing callback: receives one input item, returns an output item
 * (or NULL if this input produces no output). */
typedef void *(*ml_pipeline_stage_fn)(void *input, void *user_data);

/* Destructor for input/output items that need cleanup when evicted
 * from a full queue or when the stage is destroyed. */
typedef void  (*ml_pipeline_stage_dtor)(void *data);

typedef struct {
    ml_pipeline_stage_fn   process;         /* processing function */
    ml_pipeline_stage_dtor del_input;       /* input item destructor (optional) */
    ml_pipeline_stage_dtor del_output;      /* output item destructor (optional) */
    int                    max_queue;       /* queue capacity (default 8) */
    int                    idle_timeout_ms; /* 0 = wait forever, -1 = infinite */
    void                  *user_data;       /* passed to process callback */
    ml_pool_t             *pool;            /* memory pool for internal queues */
    ml_queue_t            *input_queue;     /* external input (optional) */
    ml_queue_t            *output_queue;    /* external output (optional) */
} ml_pipeline_stage_cfg_t;

/* ── lifecycle ─────────────────────────────────────────────────────── */

ml_pipeline_stage_t *ml_pipeline_stage_create(const ml_pipeline_stage_cfg_t *cfg);
void                 ml_pipeline_stage_destroy(ml_pipeline_stage_t *s);

/* ── control ────────────────────────────────────────────────────────── */

int ml_pipeline_stage_start(ml_pipeline_stage_t *s);
int ml_pipeline_stage_stop(ml_pipeline_stage_t *s);   /* signal thread to stop */
int ml_pipeline_stage_join(ml_pipeline_stage_t *s);   /* wait for thread exit */

/* ── data flow ──────────────────────────────────────────────────────── */

/* Push input into the stage (may block if queue is full). */
int   ml_pipeline_stage_push(ml_pipeline_stage_t *s, void *input);
/* Pop output from the stage (may block if queue is empty).
 * Returns 0 on success, ML_EAGAIN on timeout. */
int   ml_pipeline_stage_pop(ml_pipeline_stage_t *s, void **output, int timeout_ms);
int   ml_pipeline_stage_input_count(const ml_pipeline_stage_t *s);
int   ml_pipeline_stage_output_count(const ml_pipeline_stage_t *s);

/* Set external queues after creation (for connected stages).
 * Must be called before start(). Takes ownership of the queue pointer
 * only if own is non-zero. */
void ml_pipeline_stage_set_input(ml_pipeline_stage_t *s, ml_queue_t *q, int own);
void ml_pipeline_stage_set_output(ml_pipeline_stage_t *s, ml_queue_t *q, int own);

#endif /* __ML_PIPELINE_STAGE_H__ */
