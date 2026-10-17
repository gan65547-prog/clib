/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_pipeline_stage — Producer-consumer pipeline stage with its own thread.
 *
 * Each stage runs a user-provided process() callback in a dedicated thread.
 * Input items arrive via an input queue, are transformed, and results are
 * pushed to an output queue.  Bounded queues provide automatic backpressure:
 * a full output queue blocks the stage from consuming more input.
 *
 * Stop signal: ml_pipeline_stage_stop() pushes a sentinel into the input
 * queue to wake a thread blocked on ml_queue_pop(), so the thread exits
 * promptly even when configured with infinite wait.
 */

#include "ml_pipeline_stage.h"
#include "ml_pool.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

struct ml_pipeline_stage_s {
    ml_pipeline_stage_fn   process;
    ml_pipeline_stage_dtor del_input;
    ml_pipeline_stage_dtor del_output;
    void                  *user_data;
    int                    idle_timeout_ms;

    ml_queue_t            *input_q;
    ml_queue_t            *output_q;
    int                    own_input;
    int                    own_output;

    ml_pool_t             *pool;

    pthread_t              thread;
    int                    thread_running;

    pthread_mutex_t        mutex;
    char                   err_msg[128];

    /* sentinel pushed to input_q to unblock thread on stop */
    int                    stop_sentinel;

    /* set by stop() to request thread exit; breaks output-push deadlock.
     * _Atomic guarantees visibility across threads per C11 memory model. */
    _Atomic int            shutdown;
};

/* ── helpers ────────────────────────────────────────────────────────── */

static void set_err(ml_pipeline_stage_t *s, const char *msg)
{
    if (s && msg) {
        strncpy(s->err_msg, msg, sizeof(s->err_msg) - 1);
        s->err_msg[sizeof(s->err_msg) - 1] = '\0';
    }
}

static void discard_one(ml_queue_t *q, ml_pipeline_stage_dtor dtor,
                         const void *sentinel)
{
    void *item = NULL;
    if (ml_queue_pop(q, &item, 0) == ML_OK && item && item != sentinel && dtor)
        dtor(item);
}

/* ── thread function ────────────────────────────────────────────────── */

static void *stage_thread(void *arg)
{
    ml_pipeline_stage_t *s = (ml_pipeline_stage_t *)arg;
    void *input  = NULL;
    void *output = NULL;
    int   timeout;

    timeout = s->idle_timeout_ms;

    while (1) {
        int ret = ml_queue_pop(s->input_q, &input, timeout);
        if (ret != ML_OK) {
            if (ret == ML_EAGAIN && timeout > 0) continue;
            break;
        }

        /* sentinel pushed by stop() — exit cleanly */
        if (input == &s->stop_sentinel) break;

        /* process */
        output = s->process(input, s->user_data);

        if (s->del_input && input) s->del_input(input);

        if (output) {
            ret = ml_queue_push(s->output_q, output, -1);
            if (ret != ML_OK) {
                if (s->del_output) s->del_output(output);
            }
            /* check shutdown after push — stop() drains output_q to
             * unblock us, and we must exit even if push succeeded */
            if (s->shutdown) break;
        }
    }

    return NULL;
}

/* ── lifecycle ──────────────────────────────────────────────────────── */

ml_pipeline_stage_t *
ml_pipeline_stage_create(const ml_pipeline_stage_cfg_t *cfg)
{
    ml_pipeline_stage_t *s;

    if (!cfg || !cfg->process || !cfg->pool) return NULL;

    s = calloc(1, sizeof(*s));
    if (!s) return NULL;

    s->process         = cfg->process;
    s->del_input       = cfg->del_input;
    s->del_output      = cfg->del_output;
    s->user_data       = cfg->user_data;
    s->idle_timeout_ms = cfg->idle_timeout_ms > 0 ? cfg->idle_timeout_ms : -1;
    s->pool            = cfg->pool;
    s->thread_running  = 0;

    pthread_mutex_init(&s->mutex, NULL);

    if (cfg->input_queue) {
        s->input_q   = cfg->input_queue;
        s->own_input = 0;
    } else {
        int cap = cfg->max_queue > 0 ? cfg->max_queue : 8;
        s->input_q = ml_queue_create(cap, s->pool);
        if (!s->input_q) { free(s); return NULL; }
        s->own_input = 1;
    }

    if (cfg->output_queue) {
        s->output_q   = cfg->output_queue;
        s->own_output = 0;
    } else {
        int cap = cfg->max_queue > 0 ? cfg->max_queue : 8;
        s->output_q = ml_queue_create(cap, s->pool);
        if (!s->output_q) {
            if (s->own_input) ml_queue_destroy(s->input_q);
            free(s);
            return NULL;
        }
        s->own_output = 1;
    }

    return s;
}

void
ml_pipeline_stage_destroy(ml_pipeline_stage_t *s)
{
    if (!s) return;

    ml_pipeline_stage_stop(s);
    ml_pipeline_stage_join(s);

    if (s->own_input) {
        while (ml_queue_count(s->input_q) > 0)
            discard_one(s->input_q, s->del_input, &s->stop_sentinel);
        ml_queue_destroy(s->input_q);
    }
    if (s->own_output) {
        while (ml_queue_count(s->output_q) > 0)
            discard_one(s->output_q, s->del_output, NULL);
        ml_queue_destroy(s->output_q);
    }

    pthread_mutex_destroy(&s->mutex);
    free(s);
}

/* ── control ────────────────────────────────────────────────────────── */

int
ml_pipeline_stage_start(ml_pipeline_stage_t *s)
{
    if (!s) return ML_ERROR;

    s->thread_running = 1;
    if (pthread_create(&s->thread, NULL, stage_thread, s) != 0) {
        s->thread_running = 0;
        set_err(s, "pthread_create failed");
        return ML_ERROR;
    }

    return ML_OK;
}

int
ml_pipeline_stage_stop(ml_pipeline_stage_t *s)
{
    void *discard = NULL;

    if (!s) return ML_ERROR;

    s->shutdown = 1;

    /* drain output queue to unblock thread if it's stuck on push.
     * Only drain queues we own — external queues belong to the caller. */
    if (s->own_output && s->output_q) {
        while (ml_queue_pop(s->output_q, &discard, 0) == ML_OK) {
            if (s->del_output && discard) s->del_output(discard);
        }
    }

    /* push sentinel to wake thread blocked on input pop.
     * non-blocking: if queue is full, drain one slot and retry */
    if (s->input_q) {
        if (ml_queue_push(s->input_q, &s->stop_sentinel, 0) != ML_OK) {
            ml_queue_pop(s->input_q, &discard, 0);
            if (discard && discard != &s->stop_sentinel && s->del_input)
                s->del_input(discard);
            ml_queue_push(s->input_q, &s->stop_sentinel, 0);
        }
    }

    return ML_OK;
}

int
ml_pipeline_stage_join(ml_pipeline_stage_t *s)
{
    if (!s) return ML_ERROR;
    if (!s->thread_running) return ML_OK;

    pthread_join(s->thread, NULL);
    s->thread_running = 0;
    return ML_OK;
}

/* ── data flow ──────────────────────────────────────────────────────── */

int
ml_pipeline_stage_push(ml_pipeline_stage_t *s, void *input)
{
    if (!s || !s->input_q) return ML_ERROR;
    return ml_queue_push(s->input_q, input, -1);
}

int
ml_pipeline_stage_pop(ml_pipeline_stage_t *s, void **output, int timeout_ms)
{
    if (!s || !s->output_q || !output) return ML_ERROR;
    return ml_queue_pop(s->output_q, output, timeout_ms);
}

int
ml_pipeline_stage_input_count(const ml_pipeline_stage_t *s)
{
    if (!s || !s->input_q) return 0;
    return ml_queue_count(s->input_q);
}

int
ml_pipeline_stage_output_count(const ml_pipeline_stage_t *s)
{
    if (!s || !s->output_q) return 0;
    return ml_queue_count(s->output_q);
}

void
ml_pipeline_stage_set_input(ml_pipeline_stage_t *s, ml_queue_t *q, int own)
{
    if (!s) return;
    if (s->own_input && s->input_q) {
        while (ml_queue_count(s->input_q) > 0)
            discard_one(s->input_q, s->del_input, &s->stop_sentinel);
        ml_queue_destroy(s->input_q);
    }
    s->input_q   = q;
    s->own_input = own;
}

void
ml_pipeline_stage_set_output(ml_pipeline_stage_t *s, ml_queue_t *q, int own)
{
    if (!s) return;
    if (s->own_output && s->output_q) {
        while (ml_queue_count(s->output_q) > 0)
            discard_one(s->output_q, s->del_output, NULL);
        ml_queue_destroy(s->output_q);
    }
    s->output_q   = q;
    s->own_output = own;
}
