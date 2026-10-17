/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_pipeline_stage tests — create/destroy, push/pop, backpressure, cleanup.
 */
#include "test_framework.h"
#include "../include/ml_pipeline_stage.h"
#include "../include/ml_pool.h"
#include "../include/ml_queue.h"

#include <pthread.h>
#include <unistd.h>

static ml_pool_t g_pool;

/* ── test helper: pass-through processor ────────────────────────────── */

static int g_process_called  = 0;
static int g_del_input_calls  = 0;
static int g_del_output_calls = 0;

static void reset_counters(void)
{
    g_process_called   = 0;
    g_del_input_calls  = 0;
    g_del_output_calls = 0;
}

static void *identity_process(void *input, void *user_data)
{
    (void)user_data;
    g_process_called++;
    return input; /* pass through */
}

static void *increment_process(void *input, void *user_data)
{
    (void)user_data;
    g_process_called++;
    int *val = (int *)input;
    int *out = ml_alloc(sizeof(int));
    *out = *val + 1;
    return out;
}

static void count_del_input(void *data)
{
    (void)data;
    g_del_input_calls++;
}

static void count_del_output(void *data)
{
    (void)data;
    g_del_output_calls++;
}

static void free_del_output(void *data)
{
    ml_free(data);
}

/* ── create / destroy ───────────────────────────────────────────────── */

static void test_create_destroy(void)
{
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process = identity_process;
    cfg.pool    = &g_pool;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");
    ASSERT_EQ(ml_pipeline_stage_input_count(s), 0, "input count 0");
    ASSERT_EQ(ml_pipeline_stage_output_count(s), 0, "output count 0");

    ml_pipeline_stage_destroy(s);
}

static void test_create_null_process(void)
{
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.pool = &g_pool;

    ASSERT_PTR_NULL(ml_pipeline_stage_create(&cfg), "NULL process fails");
}

static void test_create_null_pool(void)
{
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process = identity_process;

    ASSERT_PTR_NULL(ml_pipeline_stage_create(&cfg), "NULL pool fails");
}

static void test_create_null_cfg(void)
{
    ASSERT_PTR_NULL(ml_pipeline_stage_create(NULL), "NULL cfg fails");
}

/* ── push / pop ─────────────────────────────────────────────────────── */

static void *slow_identity(void *input, void *user_data)
{
    (void)user_data;
    usleep(10000); /* 10ms delay to ensure items are queued */
    return input;
}

static void test_push_pop_single(void)
{
    reset_counters();
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process = slow_identity;
    cfg.pool    = &g_pool;
    cfg.max_queue = 4;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");

    ASSERT_EQ(ml_pipeline_stage_start(s), ML_OK, "start");

    int val = 42;
    ASSERT_EQ(ml_pipeline_stage_push(s, &val), ML_OK, "push");

    void *out = NULL;
    ASSERT_EQ(ml_pipeline_stage_pop(s, &out, 5000), ML_OK, "pop");
    ASSERT(out != NULL, "got output");
    ASSERT_EQ(*(int *)out, 42, "value preserved");

    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop");
    ml_pipeline_stage_join(s);
    ml_pipeline_stage_destroy(s);
}

static void test_push_pop_multiple(void)
{
    reset_counters();
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process = slow_identity;
    cfg.pool    = &g_pool;
    cfg.max_queue = 8;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");
    ASSERT_EQ(ml_pipeline_stage_start(s), ML_OK, "start");

    int vals[] = {10, 20, 30, 40, 50};
    int i;
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(ml_pipeline_stage_push(s, &vals[i]), ML_OK, "push");
    }

    for (i = 0; i < 5; i++) {
        void *out = NULL;
        ASSERT_EQ(ml_pipeline_stage_pop(s, &out, 5000), ML_OK, "pop");
        ASSERT(out != NULL, "got output");
        ASSERT_EQ(*(int *)out, vals[i], "FIFO order preserved");
    }

    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop");
    ml_pipeline_stage_join(s);
    ml_pipeline_stage_destroy(s);
}

/* ── transform ──────────────────────────────────────────────────────── */

static void test_transform_increment(void)
{
    reset_counters();
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process    = increment_process;
    cfg.del_output = free_del_output;
    cfg.pool       = &g_pool;
    cfg.max_queue  = 4;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");
    ASSERT_EQ(ml_pipeline_stage_start(s), ML_OK, "start");

    int val = 5;
    ASSERT_EQ(ml_pipeline_stage_push(s, &val), ML_OK, "push");

    void *out = NULL;
    ASSERT_EQ(ml_pipeline_stage_pop(s, &out, 5000), ML_OK, "pop");
    ASSERT(out != NULL, "got output");
    ASSERT_EQ(*(int *)out, 6, "5 + 1 = 6");
    ml_free(out);

    ASSERT(g_process_called >= 1, "process was called");

    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop");
    ml_pipeline_stage_join(s);
    ml_pipeline_stage_destroy(s);
}

/* ── stop / join ────────────────────────────────────────────────────── */

static void *blocking_process(void *input, void *user_data)
{
    (void)user_data;
    /* block until stop signal — input is just a sentinel */
    usleep(100000); /* 100ms — should be stopped before this finishes */
    return input;
}

static void test_stop_join(void)
{
    reset_counters();
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process = blocking_process;
    cfg.pool    = &g_pool;
    cfg.max_queue = 4;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");
    ASSERT_EQ(ml_pipeline_stage_start(s), ML_OK, "start");

    int val = 1;
    ASSERT_EQ(ml_pipeline_stage_push(s, &val), ML_OK, "push");

    /* give thread time to start processing */
    usleep(10000);

    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop");
    ASSERT_EQ(ml_pipeline_stage_join(s), ML_OK, "join");
    /* if we get here without hanging, stop+join works */

    ml_pipeline_stage_destroy(s);
}

/* ── external queues (connected stages) ─────────────────────────────── */

static void test_connected_stages(void)
{
    reset_counters();
    int cap = 4;

    /* shared queue between stage A and stage B */
    ml_queue_t *shared = ml_queue_create(cap, &g_pool);
    ASSERT_PTR_NOT_NULL(shared, "shared queue");

    /* stage A: identity, internal input, shared output */
    ml_pipeline_stage_cfg_t cfgA;
    memset(&cfgA, 0, sizeof(cfgA));
    cfgA.process      = identity_process;
    cfgA.pool         = &g_pool;
    cfgA.max_queue    = cap;
    cfgA.output_queue = shared;
    ml_pipeline_stage_t *A = ml_pipeline_stage_create(&cfgA);
    ASSERT_PTR_NOT_NULL(A, "create A");

    /* stage B: identity, shared input, internal output */
    ml_pipeline_stage_cfg_t cfgB;
    memset(&cfgB, 0, sizeof(cfgB));
    cfgB.process     = identity_process;
    cfgB.pool        = &g_pool;
    cfgB.max_queue   = cap;
    cfgB.input_queue = shared;
    ml_pipeline_stage_t *B = ml_pipeline_stage_create(&cfgB);
    ASSERT_PTR_NOT_NULL(B, "create B");

    ASSERT_EQ(ml_pipeline_stage_start(A), ML_OK, "start A");
    ASSERT_EQ(ml_pipeline_stage_start(B), ML_OK, "start B");

    int val = 99;
    ASSERT_EQ(ml_pipeline_stage_push(A, &val), ML_OK, "push A");

    void *out = NULL;
    ASSERT_EQ(ml_pipeline_stage_pop(B, &out, 5000), ML_OK, "pop B");
    ASSERT(out != NULL, "got output from B");
    ASSERT_EQ(*(int *)out, 99, "value through chain A→B");

    ASSERT_EQ(ml_pipeline_stage_stop(A), ML_OK, "stop A");
    ASSERT_EQ(ml_pipeline_stage_stop(B), ML_OK, "stop B");
    ml_pipeline_stage_join(A);
    ml_pipeline_stage_join(B);

    ml_pipeline_stage_destroy(A);
    ml_pipeline_stage_destroy(B);
    /* shared queue was not owned by either → not destroyed by stages */
    ml_queue_destroy(shared);
}

/* ── queue count tracking ───────────────────────────────────────────── */

static void test_input_output_counts(void)
{
    reset_counters();
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process   = slow_identity;
    cfg.pool      = &g_pool;
    cfg.max_queue = 8;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");
    ASSERT_EQ(ml_pipeline_stage_start(s), ML_OK, "start");

    ASSERT_EQ(ml_pipeline_stage_input_count(s), 0, "initial input=0");
    ASSERT_EQ(ml_pipeline_stage_output_count(s), 0, "initial output=0");

    int v1 = 1, v2 = 2;
    ml_pipeline_stage_push(s, &v1);
    ml_pipeline_stage_push(s, &v2);

    /* input count should decrease as items are consumed */
    usleep(50000); /* allow processing */

    /* output should eventually have 2 items */
    int out_count = ml_pipeline_stage_output_count(s);
    ASSERT(out_count >= 1, "output has items");

    /* drain */
    void *out = NULL;
    while (ml_pipeline_stage_pop(s, &out, 100) == ML_OK) {}

    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop");
    ml_pipeline_stage_join(s);
    ml_pipeline_stage_destroy(s);
}

/* ── destructor callbacks ───────────────────────────────────────────── */

static void test_destructors_called(void)
{
    reset_counters();
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process    = identity_process;
    cfg.del_input  = count_del_input;
    cfg.del_output = count_del_output;
    cfg.pool       = &g_pool;
    cfg.max_queue  = 4;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");
    ASSERT_EQ(ml_pipeline_stage_start(s), ML_OK, "start");

    int val = 77;
    ASSERT_EQ(ml_pipeline_stage_push(s, &val), ML_OK, "push");

    void *out = NULL;
    ASSERT_EQ(ml_pipeline_stage_pop(s, &out, 5000), ML_OK, "pop");

    ASSERT(g_del_input_calls >= 1, "del_input was called");

    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop");
    ml_pipeline_stage_join(s);
    ml_pipeline_stage_destroy(s);
}

/* ── double stop / join is safe ─────────────────────────────────────── */

static void test_double_stop(void)
{
    reset_counters();
    ml_pipeline_stage_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.process = identity_process;
    cfg.pool    = &g_pool;

    ml_pipeline_stage_t *s = ml_pipeline_stage_create(&cfg);
    ASSERT_PTR_NOT_NULL(s, "create");
    ASSERT_EQ(ml_pipeline_stage_start(s), ML_OK, "start");

    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop 1");
    ASSERT_EQ(ml_pipeline_stage_stop(s), ML_OK, "stop 2");
    ASSERT_EQ(ml_pipeline_stage_join(s), ML_OK, "join 1");
    ASSERT_EQ(ml_pipeline_stage_join(s), ML_OK, "join 2");
    /* no crash = success */

    ml_pipeline_stage_destroy(s);
}

/* ── suite ──────────────────────────────────────────────────────────── */

void test_pipeline_stage_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_pipeline_stage");

    RUN_TEST("create / destroy",           test_create_destroy());
    RUN_TEST("NULL process fails",         test_create_null_process());
    RUN_TEST("NULL pool fails",            test_create_null_pool());
    RUN_TEST("NULL cfg fails",             test_create_null_cfg());

    RUN_TEST("push / pop - single",        test_push_pop_single());
    RUN_TEST("push / pop - multiple",      test_push_pop_multiple());
    RUN_TEST("transform - increment",      test_transform_increment());

    RUN_TEST("stop / join",                test_stop_join());
    RUN_TEST("connected stages A→B",       test_connected_stages());
    RUN_TEST("input / output counts",      test_input_output_counts());
    RUN_TEST("destructors called",         test_destructors_called());
    RUN_TEST("double stop / join safe",    test_double_stop());

    ml_pool_destroy(&g_pool);
}
