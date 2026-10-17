/*
 * test_codec.c — Unit tests for ml_codec MPP encoder module.
 *
 * Tests the stub path on non-Linux; full MPP path requires RK3588 hardware.
 */
#include <string.h>
#include <stdio.h>
#include "test_framework.h"
#include "../include/ml_codec.h"
#include "../include/ml_frame.h"
#include "../include/ml_pool.h"

#if defined(__linux__) && __has_include(<rockchip/mpp_buffer.h>)
#include "../src/ml_codec_mpp.c"
#else
#include "../src/ml_codec_stub.c"
#endif

static ml_pool_t g_pool;

/* ── lifecycle ────────────────────────────────────────────────────────── */

static void test_create_destroy(void)
{
    ml_codec_t *c = ml_codec_create(NULL, &g_pool);
    /* stub returns non-NULL but encodes fail; MPP may fail without
     * /dev/mpp_service on non-Rockchip Linux */
    if (c) {
        ml_codec_destroy(c);
        /* created — may be stub or MPP init failure is OK in CI */
    }
    /* test we handle NULL destroy */
    ml_codec_destroy(NULL);
}

static void test_create_with_cfg(void)
{
    ml_codec_cfg_t cfg = ML_CODEC_CFG_DEFAULT;
    cfg.width  = 640;
    cfg.height = 480;
    cfg.bitrate = 2000000;

    ml_codec_t *c = ml_codec_create(&cfg, &g_pool);
    if (c) {
        ml_codec_destroy(c);
    }
}

/* ── error path ───────────────────────────────────────────────────────── */

static void test_null_checks(void)
{
    ml_frame_t frame;
    ml_frame_init(&frame);
    ml_frame_from_mem(&frame, NULL, 640*480*3, 640, 480,
                      ML_PIXFMT_BGR888, 0, 0);

    ASSERT(ml_codec_send_frame(NULL, &frame) < 0, "NULL c -> send_frame fails");
    ASSERT(ml_codec_send_frame(NULL, NULL) < 0, "NULL c+frame -> send_frame fails");
    ASSERT(ml_codec_receive_packet(NULL, NULL) < 0, "NULL c -> receive_packet fails");
    ASSERT(ml_codec_flush(NULL) < 0, "NULL c -> flush fails");
    ASSERT(ml_codec_get_extradata(NULL, NULL, NULL) < 0,
           "NULL c -> get_extradata fails");
}

static void test_error_str(void)
{
    ml_codec_t *c = ml_codec_create(NULL, &g_pool);
    if (c) {
        const char *err = ml_codec_error(c);
        /* stub should produce an error message */
        ASSERT_PTR_NOT_NULL(err, "error message non-NULL");
        ml_codec_destroy(c);
    }
}

/* ── backend query ────────────────────────────────────────────────────── */

static void test_backend_available(void)
{
    /* MPP should only report available on Rockchip Linux */
    int mpp = ml_codec_backend_available(ML_CODEC_BACKEND_MPP);
    ASSERT(mpp == 0 || mpp == 1, "backend_available returns 0 or 1");

    /* software should always be available */
    ASSERT(ml_codec_backend_available(ML_CODEC_BACKEND_SOFTWARE) == 1,
           "software backend always available");
}

/* ── suite ────────────────────────────────────────────────────────────── */

void test_codec_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_codec — MPP hardware encoder");

    RUN_TEST("create/destroy",          { test_create_destroy(); });
    RUN_TEST("create with config",      { test_create_with_cfg(); });
    RUN_TEST("null checks",             { test_null_checks(); });
    RUN_TEST("error string",            { test_error_str(); });
    RUN_TEST("backend query",           { test_backend_available(); });

    ml_pool_destroy(&g_pool);
}
