/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_types.h"
#include "ml_code.h"
#include "ml_pool.h"
#include "ml_thread.h"
#include "ml_thread_pool.h"

typedef struct ml_thread_task_s {
    void (*f_func)(void *);
    void                 *m_arg;
    struct ml_thread_task_s *m_next;
} ml_thread_task_t;

struct ml_thread_pool_s {
    ml_thread_mutex_t     m_mutex;
    ml_thread_cond_t      m_cond;
    ml_thread_cond_t      m_idle_cond;
    ml_thread_tid_t      *m_threads;
    ml_int_t              m_num_threads;
    ml_int_t              m_max_queue;
    ml_int_t              m_stop;
    ml_int_t              m_paused;
    ml_int_t              m_active;
    ml_int_t              m_queue_count;
    ml_thread_task_t     *m_queue_head;
    ml_thread_task_t     *m_queue_tail;
    ml_pool_t            *m_pool;
};

static void *ml_thread_pool_worker(void *arg)
{
    ml_thread_pool_t  *tp = (ml_thread_pool_t *)arg;
    ml_thread_task_t  *task;

    for (;;) {
        ml_thread_mutex_lock(&tp->m_mutex);

        while ((tp->m_queue_head == NULL || tp->m_paused) && !tp->m_stop) {
            ml_thread_cond_wait(&tp->m_cond, &tp->m_mutex);
        }

        if (tp->m_stop && tp->m_queue_head == NULL) {
            ml_thread_mutex_unlock(&tp->m_mutex);
            break;
        }

        task = tp->m_queue_head;
        tp->m_queue_head = task->m_next;
        if (tp->m_queue_head == NULL) {
            tp->m_queue_tail = NULL;
        }
        tp->m_queue_count--;
        tp->m_active++;

        ml_thread_mutex_unlock(&tp->m_mutex);

        task->f_func(task->m_arg);

        ml_thread_mutex_lock(&tp->m_mutex);
        tp->m_pool->f_free(tp->m_pool, task);
        tp->m_active--;

        if (tp->m_queue_head == NULL && tp->m_active == 0) {
            ml_thread_cond_signal(&tp->m_idle_cond);
        }

        ml_thread_mutex_unlock(&tp->m_mutex);
    }

    return NULL;
}

ml_thread_pool_t *ml_thread_pool_create(ml_pool_t *pool, ml_int_t nthreads,
                                         ml_int_t max_queue)
{
    ml_thread_pool_t *tp;
    ml_int_t           i;

    if (nthreads < 1 || pool == NULL) {
        return NULL;
    }

    tp = pool->f_alloc(pool, sizeof(ml_thread_pool_t));
    if (tp == NULL) {
        return NULL;
    }
    ml_memzero(tp, sizeof(ml_thread_pool_t));

    tp->m_threads = pool->f_alloc(pool, nthreads * sizeof(ml_thread_tid_t));
    if (tp->m_threads == NULL) {
        return NULL;
    }

    tp->m_num_threads = nthreads;
    tp->m_max_queue   = max_queue;
    tp->m_pool        = pool;

    if (ml_thread_mutex_create(&tp->m_mutex) != ML_OK)   goto fail_threads;
    if (ml_thread_cond_create(&tp->m_cond) != ML_OK)     goto fail_mutex;
    if (ml_thread_cond_create(&tp->m_idle_cond) != ML_OK) goto fail_cond;

    for (i = 0; i < nthreads; i++) {
        if (pthread_create(&tp->m_threads[i], NULL,
                          ml_thread_pool_worker, tp) != 0) {
            /* Shutdown threads already created */
            ml_thread_mutex_lock(&tp->m_mutex);
            tp->m_stop = 1;
            ml_thread_mutex_unlock(&tp->m_mutex);
            ml_thread_cond_broadcast(&tp->m_cond);

            while (--i >= 0) {
                pthread_join(tp->m_threads[i], NULL);
            }
            goto fail_idle_cond;
        }
    }

    return tp;

fail_idle_cond:
    ml_thread_cond_destroy(&tp->m_idle_cond);
fail_cond:
    ml_thread_cond_destroy(&tp->m_cond);
fail_mutex:
    ml_thread_mutex_destroy(&tp->m_mutex);
fail_threads:
    pool->f_free(pool, tp->m_threads);
    pool->f_free(pool, tp);
    return NULL;
}

void ml_thread_pool_destroy(ml_thread_pool_t *tp)
{
    ml_int_t i;

    if (tp == NULL) return;

    ml_thread_mutex_lock(&tp->m_mutex);
    tp->m_stop = 1;
    ml_thread_mutex_unlock(&tp->m_mutex);

    /* Wake all workers */
    ml_thread_cond_broadcast(&tp->m_cond);

    for (i = 0; i < tp->m_num_threads; i++) {
        pthread_join(tp->m_threads[i], NULL);
    }

    /* Free remaining queued tasks */
    while (tp->m_queue_head) {
        ml_thread_task_t *t = tp->m_queue_head;
        tp->m_queue_head = t->m_next;
        tp->m_pool->f_free(tp->m_pool, t);
    }

    ml_thread_cond_destroy(&tp->m_idle_cond);
    ml_thread_cond_destroy(&tp->m_cond);
    ml_thread_mutex_destroy(&tp->m_mutex);

    tp->m_pool->f_free(tp->m_pool, tp->m_threads);
    tp->m_pool->f_free(tp->m_pool, tp);
}

ml_int_t ml_thread_pool_submit(ml_thread_pool_t *tp, void (*func)(void *), void *arg)
{
    ml_thread_task_t *task;

    if (tp == NULL || func == NULL) return ML_ERROR;

    ml_thread_mutex_lock(&tp->m_mutex);

    if (tp->m_stop) {
        ml_thread_mutex_unlock(&tp->m_mutex);
        return ML_ERROR;
    }

    if (tp->m_max_queue > 0 && tp->m_queue_count >= tp->m_max_queue) {
        ml_thread_mutex_unlock(&tp->m_mutex);
        return ML_ERROR;
    }

    task = tp->m_pool->f_alloc(tp->m_pool, sizeof(ml_thread_task_t));
    if (task == NULL) {
        ml_thread_mutex_unlock(&tp->m_mutex);
        return ML_ERROR;
    }

    task->f_func = func;
    task->m_arg  = arg;
    task->m_next = NULL;

    if (tp->m_queue_tail) {
        tp->m_queue_tail->m_next = task;
    } else {
        tp->m_queue_head = task;
    }
    tp->m_queue_tail = task;
    tp->m_queue_count++;

    ml_thread_cond_signal(&tp->m_cond);
    ml_thread_mutex_unlock(&tp->m_mutex);

    return ML_OK;
}

void ml_thread_pool_wait(ml_thread_pool_t *tp)
{
    if (tp == NULL) return;

    ml_thread_mutex_lock(&tp->m_mutex);

    while (tp->m_queue_head != NULL || tp->m_active > 0) {
        ml_thread_cond_wait(&tp->m_idle_cond, &tp->m_mutex);
    }

    ml_thread_mutex_unlock(&tp->m_mutex);
}

void ml_thread_pool_pause(ml_thread_pool_t *tp)
{
    if (tp == NULL) return;
    ml_thread_mutex_lock(&tp->m_mutex);
    tp->m_paused = 1;
    ml_thread_mutex_unlock(&tp->m_mutex);
}

void ml_thread_pool_resume(ml_thread_pool_t *tp)
{
    if (tp == NULL) return;
    ml_thread_mutex_lock(&tp->m_mutex);
    tp->m_paused = 0;
    ml_thread_mutex_unlock(&tp->m_mutex);
    ml_thread_cond_broadcast(&tp->m_cond);
}

ml_int_t ml_thread_pool_get_working_count(ml_thread_pool_t *tp)
{
    ml_int_t count;
    if (tp == NULL) return 0;
    ml_thread_mutex_lock(&tp->m_mutex);
    count = tp->m_active;
    ml_thread_mutex_unlock(&tp->m_mutex);
    return count;
}
