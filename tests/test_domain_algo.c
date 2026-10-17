/*
 * Domain algorithm primitive tests: counter, debounce, temporal_window, range_finder.
 */
#include "test_framework.h"
#include "ml_counter.h"
#include "ml_debounce.h"
#include "ml_temporal_window.h"
#include "ml_range_finder.h"
#include <math.h>

#define ASSERT_FLOAT(a, b, eps, msg) \
    do { \
        if (fabsf((a) - (b)) > (eps)) { \
            printf("FAIL\n    [%s:%d] %s: expected %.4f got %.4f\n", \
                   __FILE__, __LINE__, msg, (double)(b), (double)(a)); \
            _tests_fail++; \
            _current_test_failed = 1; \
            return; \
        } \
    } while(0)

/* ── ml_counter ────────────────────────────────────────────────────── */

static void test_counter_init(void)
{
    ml_counter_t c;
    ml_counter_init(&c, 10.0f, 2.0f);
    ASSERT_EQ(c.count, 0, "count=0");
    ASSERT_EQ(c.armed, 1, "armed after init");
}

static void test_counter_no_cross(void)
{
    ml_counter_t c;
    ml_counter_init(&c, 10.0f, 2.0f);
    int r = ml_counter_update(&c, 5.0f);
    ASSERT_EQ(r, 0, "no trigger below threshold");
    ASSERT_EQ(ml_counter_get(&c), 0, "count still 0");
}

static void test_counter_one_cross(void)
{
    ml_counter_t c;
    ml_counter_init(&c, 10.0f, 2.0f);
    int r = ml_counter_update(&c, 12.0f);
    ASSERT_EQ(r, 1, "triggered");
    ASSERT_EQ(ml_counter_get(&c), 1, "count=1");
}

static void test_counter_hysteresis(void)
{
    ml_counter_t c;
    ml_counter_init(&c, 10.0f, 3.0f);

    /* cross up */
    int r1 = ml_counter_update(&c, 11.0f);
    ASSERT_EQ(r1, 1, "first cross");
    ASSERT_EQ(ml_counter_get(&c), 1, "count=1");

    /* stays above hysteresis */
    int r2 = ml_counter_update(&c, 8.5f);  /* 8.5 > 10-3=7, not yet re-armed */
    ASSERT_EQ(r2, 0, "no re-arm yet");

    /* drop below hysteresis band */
    int r3 = ml_counter_update(&c, 6.0f);  /* 6 < 7, re-arm */
    ASSERT_EQ(r3, 0, "re-armed but no count");
    ASSERT_EQ(c.armed, 1, "armed");

    /* cross up again */
    int r4 = ml_counter_update(&c, 11.0f);
    ASSERT_EQ(r4, 1, "second cross");
    ASSERT_EQ(ml_counter_get(&c), 2, "count=2");
}

static void test_counter_reset(void)
{
    ml_counter_t c;
    ml_counter_init(&c, 10.0f, 2.0f);
    ml_counter_update(&c, 11.0f);
    ml_counter_update(&c, 6.0f);
    ml_counter_update(&c, 11.0f);
    ASSERT_EQ(ml_counter_get(&c), 2, "count=2 before reset");
    ml_counter_reset(&c);
    ASSERT_EQ(ml_counter_get(&c), 0, "count=0 after reset");
}

/* ── ml_debounce ───────────────────────────────────────────────────── */

static void test_debounce_init(void)
{
    ml_debounce_t d;
    ml_debounce_init(&d, 3);
    ASSERT_EQ(d.output, 0, "output=0");
}

static void test_debounce_immediate(void)
{
    ml_debounce_t d;
    ml_debounce_init(&d, 1);
    int r = ml_debounce_update(&d, 1);
    ASSERT_EQ(r, 1, "immediate change with threshold=1");
    ASSERT_EQ(ml_debounce_get(&d), 1, "output=1");
}

static void test_debounce_stable(void)
{
    ml_debounce_t d;
    ml_debounce_init(&d, 3);

    /* signal=1 for 3 consecutive updates */
    ASSERT_EQ(ml_debounce_update(&d, 1), 0, "tick 1");
    ASSERT_EQ(ml_debounce_update(&d, 1), 0, "tick 2");
    ASSERT_EQ(ml_debounce_update(&d, 1), 1, "tick 3 → change");
    ASSERT_EQ(ml_debounce_get(&d), 1, "output=1");
}

static void test_debounce_glitch(void)
{
    ml_debounce_t d;
    ml_debounce_init(&d, 5);

    /* glitch: signal=1 for 2 ticks, then back to 0 */
    ml_debounce_update(&d, 1);
    ml_debounce_update(&d, 1);
    int r = ml_debounce_update(&d, 0);  /* direction changed → resets count */
    ASSERT_EQ(r, 0, "glitch filtered");
    ASSERT_EQ(ml_debounce_get(&d), 0, "output=0");
}

static void test_debounce_reset(void)
{
    ml_debounce_t d;
    ml_debounce_init(&d, 2);
    ml_debounce_update(&d, 1);
    ml_debounce_update(&d, 1);  /* output → 1 */
    ml_debounce_reset(&d);
    ASSERT_EQ(ml_debounce_get(&d), 0, "output=0 after reset");
}

/* ── ml_temporal_window ────────────────────────────────────────────── */

static void test_twin_init(void)
{
    ml_temporal_window_t w;
    ml_temporal_window_init(&w, 10);
    ASSERT_EQ(ml_temporal_window_count(&w), 0u, "count=0");
    ASSERT(ml_temporal_window_ready(&w) == 0, "not ready");
    ml_temporal_window_free(&w);
}

static void test_twin_push(void)
{
    ml_temporal_window_t w;
    ml_temporal_window_init(&w, 3);

    ml_temporal_window_push(&w, 1.0f);
    ASSERT_EQ(ml_temporal_window_count(&w), 1u, "count=1");

    ml_temporal_window_push(&w, 2.0f);
    ASSERT_EQ(ml_temporal_window_count(&w), 2u, "count=2");

    ml_temporal_window_push(&w, 3.0f);
    ASSERT_EQ(ml_temporal_window_count(&w), 3u, "count=3");
    ASSERT(ml_temporal_window_ready(&w) == 1, "ready when full");

    /* push beyond capacity — oldest drops */
    ml_temporal_window_push(&w, 4.0f);
    ASSERT_EQ(ml_temporal_window_count(&w), 3u, "still 3");
    ASSERT_FLOAT(ml_temporal_window_get(&w, 0), 2.0f, 0.001f, "first=2 (dropped 1)");

    ml_temporal_window_free(&w);
}

static void test_twin_stats(void)
{
    ml_temporal_window_t w;
    ml_temporal_window_init(&w, 4);

    ml_temporal_window_push(&w, 1.0f);
    ml_temporal_window_push(&w, 2.0f);
    ml_temporal_window_push(&w, 3.0f);
    ml_temporal_window_push(&w, 4.0f);

    ASSERT_FLOAT(ml_temporal_window_sum(&w),  10.0f, 0.001f, "sum=10");
    ASSERT_FLOAT(ml_temporal_window_mean(&w), 2.5f, 0.001f, "mean=2.5");
    ASSERT_FLOAT(ml_temporal_window_min(&w),  1.0f, 0.001f, "min=1");
    ASSERT_FLOAT(ml_temporal_window_max(&w),  4.0f, 0.001f, "max=4");

    ml_temporal_window_free(&w);
}

/* ── ml_range_finder ───────────────────────────────────────────────── */

static void test_rf_init(void)
{
    ml_range_finder_t rf;
    ml_range_finder_init(&rf, 1.0f, 2);
    ASSERT_EQ(rf.direction, 1, "looking for peak");
    ASSERT_EQ(rf.initialized, 0, "not initialized");
}

static void test_rf_peak(void)
{
    ml_range_finder_t rf;
    ml_range_finder_init(&rf, 1.0f, 1);

    /* rising to peak */
    ml_range_finder_update(&rf, 1.0f);
    ml_range_finder_update(&rf, 3.0f);
    ml_range_finder_update(&rf, 5.0f);
    /* falling → should trigger peak */
    int r = ml_range_finder_update(&rf, 3.0f);
    ASSERT_EQ(r, 1, "peak detected at 5.0");
}

static void test_rf_valley(void)
{
    ml_range_finder_t rf;
    ml_range_finder_init(&rf, 1.0f, 1);

    /* rising to build last_peak */
    ml_range_finder_update(&rf, 0.0f);
    ml_range_finder_update(&rf, 4.0f);
    /* falling → peak at 4.0 */
    ml_range_finder_update(&rf, 3.0f);

    /* continue falling */
    ml_range_finder_update(&rf, 2.0f);
    ml_range_finder_update(&rf, 1.0f);
    /* rise → valley */
    int r = ml_range_finder_update(&rf, 3.0f);
    ASSERT_EQ(r, -1, "valley detected at 1.0");
}

static void test_rf_noise_reject(void)
{
    ml_range_finder_t rf;
    ml_range_finder_init(&rf, 5.0f, 1);  /* need 5.0 vertical change */

    ml_range_finder_update(&rf, 0.0f);
    ml_range_finder_update(&rf, 2.0f);
    /* only 2.0 rise, below threshold */
    int r = ml_range_finder_update(&rf, 1.0f);
    ASSERT_EQ(r, 0, "noise rejected — below threshold");
}

static void test_rf_reset(void)
{
    ml_range_finder_t rf;
    ml_range_finder_init(&rf, 1.0f, 1);

    ml_range_finder_update(&rf, 0.0f);
    ml_range_finder_update(&rf, 5.0f);
    ml_range_finder_update(&rf, 3.0f);  /* peak */
    ml_range_finder_reset(&rf);
    ASSERT_EQ(rf.initialized, 0, "reset");
}

/* ── suite ─────────────────────────────────────────────────────────── */

void test_domain_algo_suite(void)
{
    TEST_SUITE("domain_algo");

    RUN_TEST("counter - init",             test_counter_init());
    RUN_TEST("counter - no cross",         test_counter_no_cross());
    RUN_TEST("counter - one cross",        test_counter_one_cross());
    RUN_TEST("counter - hysteresis",       test_counter_hysteresis());
    RUN_TEST("counter - reset",            test_counter_reset());

    RUN_TEST("debounce - init",            test_debounce_init());
    RUN_TEST("debounce - immediate",       test_debounce_immediate());
    RUN_TEST("debounce - stable",          test_debounce_stable());
    RUN_TEST("debounce - glitch filter",   test_debounce_glitch());
    RUN_TEST("debounce - reset",           test_debounce_reset());

    RUN_TEST("temporal_window - init",     test_twin_init());
    RUN_TEST("temporal_window - push",     test_twin_push());
    RUN_TEST("temporal_window - stats",    test_twin_stats());

    RUN_TEST("range_finder - init",        test_rf_init());
    RUN_TEST("range_finder - peak",        test_rf_peak());
    RUN_TEST("range_finder - valley",      test_rf_valley());
    RUN_TEST("range_finder - noise rej",   test_rf_noise_reject());
    RUN_TEST("range_finder - reset",       test_rf_reset());
}
