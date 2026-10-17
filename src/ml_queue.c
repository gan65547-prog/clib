/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * Bounded blocking queue — mutex + two condition variables over a
 * circular buffer of void* pointers.
 */

#include "ml_queue.h"
#include "ml_pool.h"

#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <string.h>

struct ml_queue_s {
    void          **ring;        /* circular buffer of void*     */
    int             capacity;    /* max items                    */
    int             count;       /* current items                */
    int             head;        /* next read index              */
    int             tail;        /* next write index             */
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;   /* signaled on push             */
    pthread_cond_t  not_full;    /* signaled on pop              */
    ml_pool_t      *pool;
};

/* ── helpers ─────────────────────────────────────────────────────── */

static void
timespec_add_ms(struct timespec *ts, int ms)
{
    ts->tv_sec  += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec  += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

/* ── lifecycle ───────────────────────────────────────────────────── */

ml_queue_t *
ml_queue_create(int capacity, ml_pool_t *pool)
{
    ml_queue_t *q;

    if (capacity < 1 || !pool) return NULL;

    q = pool->f_alloc(pool, sizeof(*q));
    if (!q) return NULL;
    memset(q, 0, sizeof(*q));

    q->ring = pool->f_alloc(pool, (size_t)capacity * sizeof(void *));
    if (!q->ring) {
        pool->f_free(pool, q);
        return NULL;
    }

    q->capacity = capacity;
    q->count    = 0;
    q->head     = 0;
    q->tail     = 0;
    q->pool     = pool;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);

    return q;
}

void
ml_queue_destroy(ml_queue_t *q)
{
    if (!q) return;

    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    pthread_mutex_destroy(&q->mutex);

    q->pool->f_free(q->pool, q->ring);
    q->pool->f_free(q->pool, q);
}

/* ── push ────────────────────────────────────────────────────────── */

int
ml_queue_push(ml_queue_t *q, void *item, int timeout_ms)
{
    int ret = ML_OK;

    if (!q) return ML_ERROR;

    pthread_mutex_lock(&q->mutex);

    if (timeout_ms < 0) {
        /* infinite wait */
        while (q->count == q->capacity)
            pthread_cond_wait(&q->not_full, &q->mutex);
    } else if (timeout_ms == 0) {
        /* try once */
        if (q->count == q->capacity) {
            ret = ML_EAGAIN;
            goto done;
        }
    } else {
        /* timed wait */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        timespec_add_ms(&ts, timeout_ms);

        while (q->count == q->capacity) {
            int rc = pthread_cond_timedwait(&q->not_full, &q->mutex, &ts);
            if (rc == ETIMEDOUT) {
                ret = ML_EAGAIN;
                goto done;
            }
        }
    }

    q->ring[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);

done:
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

/* ── pop ─────────────────────────────────────────────────────────── */

int
ml_queue_pop(ml_queue_t *q, void **item, int timeout_ms)
{
    int ret = ML_OK;

    if (!q || !item) return ML_ERROR;

    pthread_mutex_lock(&q->mutex);

    if (timeout_ms < 0) {
        /* infinite wait */
        while (q->count == 0)
            pthread_cond_wait(&q->not_empty, &q->mutex);
    } else if (timeout_ms == 0) {
        /* try once */
        if (q->count == 0) {
            ret = ML_EAGAIN;
            goto done;
        }
    } else {
        /* timed wait */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        timespec_add_ms(&ts, timeout_ms);

        while (q->count == 0) {
            int rc = pthread_cond_timedwait(&q->not_empty, &q->mutex, &ts);
            if (rc == ETIMEDOUT) {
                ret = ML_EAGAIN;
                goto done;
            }
        }
    }

    *item = q->ring[q->head];
    q->ring[q->head] = NULL;  /* avoid stale pointer */
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);

done:
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

/* ── query ───────────────────────────────────────────────────────── */

int
ml_queue_count(const ml_queue_t *q)
{
    int c;
    if (!q) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&q->mutex);
    c = q->count;
    pthread_mutex_unlock((pthread_mutex_t *)&q->mutex);
    return c;
}

int
ml_queue_capacity(const ml_queue_t *q)
{
    return q ? q->capacity : 0;
}

bool
ml_queue_empty(const ml_queue_t *q)
{
    return ml_queue_count(q) == 0;
}

bool
ml_queue_full(const ml_queue_t *q)
{
    if (!q) return false;
    return ml_queue_count(q) == q->capacity;
}
