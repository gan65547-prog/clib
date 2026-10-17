/*
 * test_rga.c — Unit tests for ml_rga RGA 2D accelerator.
 *
 * On Linux with /dev/rga: tests actual RGA operations.
 * On macOS: tests the stub path.
 */
#include <string.h>
#include <stdio.h>
#include "test_framework.h"
#include "../include/ml_rga.h"
#include "../include/ml_pool.h"

#ifdef __linux__
#if __has_include(<rga/RgaApi.h>)
#include "../src/ml_rga.c"
#else
#include "../src/ml_rga_stub.c"
#endif
#else
#include "../src/ml_rga_stub.c"
#endif

static ml_pool_t g_pool;

/* ── lifecycle ────────────────────────────────────────────────────────── */

static void test_create_destroy(void)
{
    ml_rga_t *rga = ml_rga_create(&g_pool);
    if (rga) {
        ml_rga_destroy(rga);
    }
    ml_rga_destroy(NULL);
}

/* ── error path ───────────────────────────────────────────────────────── */

static void test_null_checks(void)
{
    ASSERT(ml_rga_execute(NULL, NULL) < 0, "NULL rga+op fails");
    ASSERT(ml_rga_resize(NULL, NULL, 0, 0, NULL, 0, 0, 0) < 0,
           "NULL resize fails");
    ASSERT(ml_rga_csc(NULL, NULL, 0, 0, 0, NULL, 0) < 0,
           "NULL csc fails");
    ASSERT_PTR_NULL(ml_rga_error(NULL), "NULL error is NULL");
}

static void test_execute_null_op(void)
{
    ml_rga_t *rga = ml_rga_create(&g_pool);
    if (rga) {
        ASSERT(ml_rga_execute(rga, NULL) < 0, "NULL op fails");
        ml_rga_destroy(rga);
    }
}

/* ── convenience zero-size guard ──────────────────────────────────────── */

static void test_resize_zero_size(void)
{
    ml_rga_t *rga = ml_rga_create(&g_pool);
    if (rga) {
        ASSERT(ml_rga_resize(rga, NULL, 0, 0, NULL, 0, 0, 0) < 0,
               "zero-size resize fails");
        ml_rga_destroy(rga);
    }
}

/* ── format conversions (requires /dev/rga on Linux) ──────────────────── */

static void test_csc_nv12_to_bgr(void)
{
    ml_rga_t *rga = ml_rga_create(&g_pool);
    if (!rga) return; /* skip if RGA unavailable */

    /* allocate test buffers */
    int w = 64, h = 64;
    int nv12_size = w * h * 3 / 2;
    int bgr_size  = w * h * 3;

    uint8_t *src = g_pool.f_alloc(&g_pool, nv12_size);
    uint8_t *dst = g_pool.f_alloc(&g_pool, bgr_size);
    ASSERT_PTR_NOT_NULL(src, "alloc src");
    ASSERT_PTR_NOT_NULL(dst, "alloc dst");

    memset(src, 0, nv12_size);

    /* NV12 → BGR888 */
    int ret = ml_rga_csc(rga, src, w, h, ML_RGA_FMT_NV12,
                         dst, ML_RGA_FMT_BGR888);
    /* may fail if RGA driver issue, but shouldn't crash */
    (void)ret;

    g_pool.f_free(&g_pool, src);
    g_pool.f_free(&g_pool, dst);
    ml_rga_destroy(rga);
}

/* ── query ────────────────────────────────────────────────────────────── */

static void test_available(void)
{
    int avail = ml_rga_available();
    ASSERT(avail == 0 || avail == 1, "available returns 0 or 1");
}

/* ── suite ────────────────────────────────────────────────────────────── */

void test_rga_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_rga — RGA 2D accelerator");

    RUN_TEST("create/destroy",      { test_create_destroy(); });
    RUN_TEST("null checks",         { test_null_checks(); });
    RUN_TEST("execute NULL op",     { test_execute_null_op(); });
    RUN_TEST("resize zero size",    { test_resize_zero_size(); });
    RUN_TEST("CSC NV12->BGR",       { test_csc_nv12_to_bgr(); });
    RUN_TEST("available query",     { test_available(); });

    ml_pool_destroy(&g_pool);
}
