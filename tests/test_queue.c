/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_queue tests — bounded queue, blocking/timeout/thread-safety.
 */
#include "test_framework.h"
#include "../include/ml_queue.h"
#include "../include/ml_pool.h"
#include "../src/ml_queue.c"

#include <pthread.h>
#include <unistd.h>

static ml_pool_t g_pool;

/* ── basic ───────────────────────────────────────────────────────── */

static void test_create_destroy(void)
{
    ml_queue_t *q = ml_queue_create(8, &g_pool);
    ASSERT_PTR_NOT_NULL(q, "create queue");
    ASSERT(ml_queue_empty(q), "empty after create");
    ASSERT(!ml_queue_full(q), "not full after create");
    ASSERT_EQ(ml_queue_count(q), 0, "count 0 after create");
    ASSERT_EQ(ml_queue_capacity(q), 8, "capacity matches");
    ml_queue_destroy(q);
}

static void test_push_pop_fifo(void)
{
    ml_queue_t *q = ml_queue_create(4, &g_pool);
    void *item = NULL;
    int items[] = {10, 20, 30, 40};

    ASSERT_PTR_NOT_NULL(q, "create");

    ASSERT_EQ(ml_queue_push(q, &items[0], -1), ML_OK, "push 1");
    ASSERT_EQ(ml_queue_push(q, &items[1], -1), ML_OK, "push 2");
    ASSERT_EQ(ml_queue_push(q, &items[2], -1), ML_OK, "push 3");
    ASSERT_EQ(ml_queue_push(q, &items[3], -1), ML_OK, "push 4");
    ASSERT(ml_queue_full(q), "full after 4 pushes");
    ASSERT_EQ(ml_queue_count(q), 4, "count 4");

    ASSERT_EQ(ml_queue_pop(q, &item, -1), ML_OK, "pop 1");
    ASSERT_EQ(*(int *)item, 10, "FIFO: first in = 10");
    ASSERT_EQ(ml_queue_pop(q, &item, -1), ML_OK, "pop 2");
    ASSERT_EQ(*(int *)item, 20, "FIFO: second = 20");
    ASSERT_EQ(ml_queue_pop(q, &item, -1), ML_OK, "pop 3");
    ASSERT_EQ(*(int *)item, 30, "FIFO: third = 30");
    ASSERT_EQ(ml_queue_pop(q, &item, -1), ML_OK, "pop 4");
    ASSERT_EQ(*(int *)item, 40, "FIFO: fourth = 40");
    ASSERT(ml_queue_empty(q), "empty after all pops");

    ml_queue_destroy(q);
}

static void test_try_push_pop(void)
{
    ml_queue_t *q = ml_queue_create(1, &g_pool);
    void *item = NULL;
    int val = 99;

    ASSERT_PTR_NOT_NULL(q, "create cap=1");

    ASSERT_EQ(ml_queue_try_push(q, &val), ML_OK, "try_push ok");
    ASSERT(ml_queue_full(q), "full");

    ASSERT_EQ(ml_queue_try_push(q, &val), ML_EAGAIN, "try_push full → EAGAIN");

    ASSERT_EQ(ml_queue_try_pop(q, &item), ML_OK, "try_pop ok");
    ASSERT_EQ(*(int *)item, 99, "popped value matches");

    ASSERT_EQ(ml_queue_try_pop(q, &item), ML_EAGAIN, "try_pop empty → EAGAIN");

    ml_queue_destroy(q);
}

static void test_timeout(void)
{
    ml_queue_t *q = ml_queue_create(1, &g_pool);
    void *item = NULL;
    int val = 42;

    ASSERT_PTR_NOT_NULL(q, "create");

    /* fill the queue */
    ASSERT_EQ(ml_queue_push(q, &val, -1), ML_OK, "fill");

    /* push with timeout on full queue */
    ASSERT_EQ(ml_queue_push(q, &val, 100), ML_EAGAIN, "push timeout → EAGAIN");

    /* pop with timeout on empty queue */
    ASSERT_EQ(ml_queue_pop(q, &item, -1), ML_OK, "drain");
    ASSERT_EQ(ml_queue_pop(q, &item, 100), ML_EAGAIN, "pop timeout → EAGAIN");

    ml_queue_destroy(q);
}

/* ── multi-threaded ──────────────────────────────────────────────── */

typedef struct {
    ml_queue_t *q;
    int        *items;
    int         count;
} thr_ctx_t;

static void *producer_thread(void *arg)
{
    thr_ctx_t *ctx = (thr_ctx_t *)arg;
    int i;
    for (i = 0; i < ctx->count; i++) {
        int ret = ml_queue_push(ctx->q, &ctx->items[i], -1);
        if (ret != ML_OK) return (void *)(intptr_t)1;
    }
    return (void *)0;
}

static void *consumer_thread(void *arg)
{
    thr_ctx_t *ctx = (thr_ctx_t *)arg;
    int i;
    for (i = 0; i < ctx->count; i++) {
        void *item = NULL;
        int ret = ml_queue_pop(ctx->q, &item, -1);
        if (ret != ML_OK) return (void *)(intptr_t)1;
        if (*(int *)item != ctx->items[i])
            return (void *)(intptr_t)2;
    }
    return (void *)0;
}

static void test_single_producer_consumer(void)
{
    ml_queue_t *q = ml_queue_create(4, &g_pool);
    pthread_t   prod, cons;
    thr_ctx_t   pctx, cctx;
    int         src[100], dst[100];
    int         i;

    ASSERT_PTR_NOT_NULL(q, "create");

    for (i = 0; i < 100; i++) { src[i] = i; dst[i] = i; }

    pctx.q = q;  pctx.items = src;  pctx.count = 100;
    cctx.q = q;  cctx.items = dst;  cctx.count = 100;

    pthread_create(&cons, NULL, consumer_thread, &cctx);
    pthread_create(&prod, NULL, producer_thread, &pctx);

    void *pres = NULL, *cres = NULL;
    pthread_join(prod, &pres);
    pthread_join(cons, &cres);

    ASSERT_PTR_NULL(pres, "producer no error");
    ASSERT_PTR_NULL(cres, "consumer no error");

    ml_queue_destroy(q);
}

typedef struct {
    ml_queue_t  *q;
    int          total;
    int          sum;
} mpc_ctx_t;

static void *mpc_producer(void *arg)
{
    mpc_ctx_t *ctx = (mpc_ctx_t *)arg;
    int *items = g_pool.f_alloc(&g_pool, (size_t)ctx->total * sizeof(int));
    int i;
    for (i = 0; i < ctx->total; i++) {
        items[i] = i + ctx->sum;  /* unique range per producer */
        if (ml_queue_push(ctx->q, &items[i], -1) != ML_OK) {
            return (void *)(intptr_t)1;
        }
    }
    return (void *)0;
}

static void *mpc_consumer(void *arg)
{
    mpc_ctx_t *ctx = (mpc_ctx_t *)arg;
    int i;
    for (i = 0; i < ctx->total; i++) {
        void *item = NULL;
        if (ml_queue_pop(ctx->q, &item, -1) != ML_OK)
            return (void *)(intptr_t)1;
        ctx->sum += *(int *)item;  /* accumulate sum */
    }
    return (void *)0;
}

static void test_multi_producer_consumer(void)
{
    ml_queue_t *q = ml_queue_create(8, &g_pool);
    pthread_t   threads[4];
    mpc_ctx_t   ctx[4];
    int         i, expected_sum = 0;

    ASSERT_PTR_NOT_NULL(q, "create");

    /* 2 producers push items 0..49 (sum=1225) and 50..99 (sum=3725) */
    /* 2 consumers each pop 50 items and accumulate sums */
    ctx[0].q = q; ctx[0].total = 50; ctx[0].sum = 0;    /* consumer */
    ctx[1].q = q; ctx[1].total = 50; ctx[1].sum = 0;    /* consumer */
    ctx[2].q = q; ctx[2].total = 50; ctx[2].sum = 0;    /* producer: items 0..49 */
    ctx[3].q = q; ctx[3].total = 50; ctx[3].sum = 50;   /* producer: items 50..99 */

    pthread_create(&threads[0], NULL, mpc_consumer, &ctx[0]);
    pthread_create(&threads[1], NULL, mpc_consumer, &ctx[1]);
    pthread_create(&threads[2], NULL, mpc_producer, &ctx[2]);
    pthread_create(&threads[3], NULL, mpc_producer, &ctx[3]);

    for (i = 0; i < 4; i++) {
        void *res = NULL;
        pthread_join(threads[i], &res);
        ASSERT_PTR_NULL(res, "thread no error");
    }

    /* sum of 0..99 = 99*100/2 = 4950 */
    expected_sum = ctx[2].sum * 2;  /* 0+50 = 50, sum 0..49 = 1225 */
    (void)expected_sum;
    ASSERT_EQ(ctx[0].sum + ctx[1].sum, 4950, "total sum matches 0..99");

    ml_queue_destroy(q);
}

/* ── null checks ─────────────────────────────────────────────────── */

static void test_null_checks(void)
{
    void *item = NULL;

    ASSERT_EQ(ml_queue_push(NULL, &item, 0), ML_ERROR, "push NULL q");
    ASSERT_EQ(ml_queue_pop(NULL, &item, 0), ML_ERROR, "pop NULL q");
    ASSERT_EQ(ml_queue_pop((ml_queue_t *)&g_pool, NULL, 0), ML_ERROR, "pop NULL item");
    ASSERT_EQ(ml_queue_count(NULL), 0, "count NULL q");
    ASSERT(ml_queue_empty(NULL), "empty NULL q → true");
    ASSERT(!ml_queue_full(NULL), "full NULL q → false");
}

/* ── interrupt ────────────────────────────────────────────────────── */

typedef struct {
    ml_queue_t *q;
    int         called;
} int_ctx_t;

static void *blocked_consumer(void *arg)
{
    int_ctx_t *ctx = (int_ctx_t *)arg;
    void *item = NULL;
    ctx->called = 1;
    int ret = ml_queue_pop(ctx->q, &item, -1);
    return (void *)(intptr_t)(ret == ML_ERROR ? 0 : 1);
}

static void *blocked_producer(void *arg)
{
    int_ctx_t *ctx = (int_ctx_t *)arg;
    int val = 42;
    ctx->called = 1;
    int ret = ml_queue_push(ctx->q, &val, -1);
    return (void *)(intptr_t)(ret == ML_ERROR ? 0 : 1);
}

static void *timed_consumer(void *arg)
{
    int_ctx_t *ctx = (int_ctx_t *)arg;
    void *item = NULL;
    ctx->called = 1;
    int ret = ml_queue_pop(ctx->q, &item, 5000);
    return (void *)(intptr_t)(ret == ML_ERROR ? 0 : 1);
}

static void test_interrupt_pop(void)
{
    ml_queue_t *q = ml_queue_create(4, &g_pool);
    pthread_t   thr;
    int_ctx_t   ctx;
    void       *res = NULL;

    ASSERT_PTR_NOT_NULL(q, "create");

    ctx.q      = q;
    ctx.called = 0;

    pthread_create(&thr, NULL, blocked_consumer, &ctx);

    /* wait for thread to enter pop */
    while (!ctx.called) usleep(1000);

    ml_queue_interrupt(q);

    pthread_join(thr, &res);
    ASSERT_PTR_NULL(res, "interrupt pop returned ML_ERROR");

    /* push/pop should still work after interrupt (interrupted flag persists,
     * so infinite-wait paths return ML_ERROR; use try variants) */
    int val = 99;
    ASSERT_EQ(ml_queue_try_push(q, &val), ML_OK, "try_push after interrupt");
    ASSERT_EQ(ml_queue_try_pop(q, (void**)&res), ML_OK, "try_pop after interrupt");

    ml_queue_destroy(q);
}

static void test_interrupt_push(void)
{
    ml_queue_t *q = ml_queue_create(1, &g_pool);
    pthread_t   thr;
    int_ctx_t   ctx;
    int         val = 1;
    void       *res = NULL;

    ASSERT_PTR_NOT_NULL(q, "create cap=1");

    /* fill the queue */
    ASSERT_EQ(ml_queue_push(q, &val, -1), ML_OK, "fill queue");

    ctx.q      = q;
    ctx.called = 0;

    pthread_create(&thr, NULL, blocked_producer, &ctx);

    while (!ctx.called) usleep(1000);

    ml_queue_interrupt(q);

    pthread_join(thr, &res);
    ASSERT_PTR_NULL(res, "interrupt push returned ML_ERROR");

    ml_queue_destroy(q);
}

static void test_interrupt_timed_wait(void)
{
    ml_queue_t *q = ml_queue_create(4, &g_pool);
    pthread_t   thr;
    int_ctx_t   ctx;
    void       *res = NULL;

    ASSERT_PTR_NOT_NULL(q, "create");

    ctx.q      = q;
    ctx.called = 0;

    pthread_create(&thr, NULL, timed_consumer, &ctx);

    while (!ctx.called) usleep(1000);

    ml_queue_interrupt(q);

    pthread_join(thr, &res);
    ASSERT_PTR_NULL(res, "interrupt timed pop returned ML_ERROR");

    ml_queue_destroy(q);
}

static void test_interrupt_null(void)
{
    ml_queue_interrupt(NULL);  /* no-op, no crash */
}

/* ── suite entry ─────────────────────────────────────────────────── */

void test_queue_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_queue — thread-safe bounded queue");

    RUN_TEST("create/destroy",        test_create_destroy());
    RUN_TEST("push/pop FIFO",         test_push_pop_fifo());
    RUN_TEST("try_push/pop",          test_try_push_pop());
    RUN_TEST("timeout",               test_timeout());
    RUN_TEST("single prod/cons 100",  test_single_producer_consumer());
    RUN_TEST("multi prod/cons 50x4",  test_multi_producer_consumer());
    RUN_TEST("null checks",           test_null_checks());
    RUN_TEST("interrupt pop",          test_interrupt_pop());
    RUN_TEST("interrupt push",         test_interrupt_push());
    RUN_TEST("interrupt timed wait",   test_interrupt_timed_wait());
    RUN_TEST("interrupt null",         test_interrupt_null());

    ml_pool_destroy(&g_pool);
}
