/*
 * test_camera.c — Unit tests for ml_camera module.
 *
 * Tests the stub on non-Linux; V4L2/RTSP path requires Linux with a camera.
 */
#include <string.h>
#include <stdio.h>
#include "test_framework.h"
#include "../include/ml_camera.h"
#include "../include/ml_frame.h"
#include "../include/ml_pool.h"

/* Backend availability flags must be defined before the dispatcher include. */
#if defined(__linux__)
 #define ML_CAMERA_HAS_V4L2 1
#endif
#if defined(__linux__) && __has_include(<libavformat/avformat.h>)
 #define ML_CAMERA_HAS_RTSP 1
#endif

/* The dispatcher (ml_camera.c) provides the public API.
 * Backend .c files provide the internal ml_camera_{v4l2,rtsp,stub}_ impls.
 * struct ml_camera_s is defined once in ml_camera_private.h (unified). */
#include "../src/ml_camera.c"
#if defined(ML_CAMERA_HAS_V4L2)
 #include "../src/ml_camera_v4l2.c"
#endif
#if defined(ML_CAMERA_HAS_RTSP)
 #include "../src/ml_camera_rtsp.c"
#endif
#include "../src/ml_camera_stub.c"

/* ── lifecycle ────────────────────────────────────────────────────────── */

static void test_create_destroy_null(void)
{
    ASSERT_PTR_NULL(ml_camera_create(NULL, NULL), "NULL cfg -> NULL");
    ml_camera_destroy(NULL); /* safe on NULL */
}

static void test_create_stub(void)
{
    ml_camera_cfg_t cfg = ML_CAMERA_CFG_DEFAULT;
    ml_camera_t *cam = ml_camera_create(&cfg, NULL);

    if (cam) {
        /* stub creates but all ops fail */
        ASSERT(ml_camera_get_state(cam) == ML_CAMERA_STATE_STOPPED,
               "initial state is STOPPED");
        const char *err = ml_camera_error(cam);
        ASSERT_PTR_NOT_NULL(err, "stub has error message");
        ml_camera_destroy(cam);
    }
    /* on Linux without /dev/video0, create may legitimately fail */
}

/* ── state transitions ────────────────────────────────────────────────── */

static void test_state_errors(void)
{
    ml_camera_cfg_t cfg = ML_CAMERA_CFG_DEFAULT;
    ml_frame_t frame;
    ml_camera_t *cam = ml_camera_create(&cfg, NULL);

    if (!cam) return; /* no camera device available */

    /* read without start should fail */
    ASSERT(ml_camera_read(cam, &frame, 0) < 0, "read before start fails");

    /* set callback while started should fail (but we haven't started yet) */
    ml_camera_destroy(cam);
}

/* ── query functions ──────────────────────────────────────────────────── */

static void test_query_null(void)
{
    ASSERT(ml_camera_get_width(NULL) < 0, "get_width(NULL) fails");
    ASSERT(ml_camera_get_height(NULL) < 0, "get_height(NULL) fails");
    ASSERT(ml_camera_get_fps(NULL) < 0, "get_fps(NULL) fails");
    ASSERT(ml_camera_get_state(NULL) < 0, "get_state(NULL) fails");
    ASSERT_PTR_NULL(ml_camera_error(NULL), "error(NULL) is NULL");
}

/* ── configuration ────────────────────────────────────────────────────── */

static void test_cfg_defaults(void)
{
    ml_camera_cfg_t cfg = ML_CAMERA_CFG_DEFAULT;
    ASSERT(cfg.backend == ML_CAMERA_BACKEND_V4L2, "default backend V4L2");
    ASSERT(cfg.width == 1920 && cfg.height == 1080, "default 1080p");
    ASSERT(cfg.buffer_count == 4, "default 4 buffers");
    ASSERT(cfg.timeout_ms == 3000, "default 3s timeout");
}

/* ── suite ────────────────────────────────────────────────────────────── */

void test_camera_suite(void)
{
    TEST_SUITE("ml_camera — V4L2 / RTSP camera abstraction");

    RUN_TEST("create NULL cfg",       { test_create_destroy_null(); });
    RUN_TEST("create stub",           { test_create_stub(); });
    RUN_TEST("state errors",          { test_state_errors(); });
    RUN_TEST("query NULL",            { test_query_null(); });
    RUN_TEST("default config",        { test_cfg_defaults(); });
}
