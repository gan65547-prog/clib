#include "test_framework.h"
#include "ml_cv_math.h"
#include "../include/ml_pool.h"
#include <math.h>

static ml_pool_t g_pool;

static int float_eq(float a, float b, float eps)
{
    return fabsf(a - b) < eps;
}

#define ASSERT_FLOAT_EQ(a, b, eps, msg) \
    do { \
        if (!float_eq((a), (b), (eps))) { \
            printf("FAIL\n    [%s:%d] %s: expected %.6f got %.6f\n", \
                   __FILE__, __LINE__, msg, (double)(b), (double)(a)); \
            _tests_fail++; \
            _current_test_failed = 1; \
            return; \
        } \
    } while(0)

#define ASSERT_FLOAT_ZERO(a, msg) \
    ASSERT_FLOAT_EQ(a, 0.0f, 0.0001f, msg)

/* ── rect_inter ─────────────────────────────────────────────────── */

static void test_rect_inter_overlap(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {5, 5, 10, 10};
    ml_rect_t r = ml_rect_inter(a, b);
    ASSERT_FLOAT_EQ(r.x,      5.0f, 0.001f, "x");
    ASSERT_FLOAT_EQ(r.y,      5.0f, 0.001f, "y");
    ASSERT_FLOAT_EQ(r.width,  5.0f, 0.001f, "width");
    ASSERT_FLOAT_EQ(r.height, 5.0f, 0.001f, "height");
}

static void test_rect_inter_no_overlap(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {20, 20, 10, 10};
    ml_rect_t r = ml_rect_inter(a, b);
    ASSERT_FLOAT_EQ(r.width,  0.0f, 0.001f, "width");
    ASSERT_FLOAT_EQ(r.height, 0.0f, 0.001f, "height");
}

static void test_rect_inter_contained(void)
{
    ml_rect_t a = {0, 0, 20, 20};
    ml_rect_t b = {5, 5, 5, 5};
    ml_rect_t r = ml_rect_inter(a, b);
    ASSERT_FLOAT_EQ(r.width,  5.0f, 0.001f, "width");
    ASSERT_FLOAT_EQ(r.height, 5.0f, 0.001f, "height");
}

/* ── rect_unite ─────────────────────────────────────────────────── */

static void test_rect_union_overlap(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {5, 5, 10, 10};
    ml_rect_t r = ml_rect_union(a, b);
    ASSERT_FLOAT_EQ(r.x,      0.0f, 0.001f, "x");
    ASSERT_FLOAT_EQ(r.y,      0.0f, 0.001f, "y");
    ASSERT_FLOAT_EQ(r.width,  15.0f, 0.001f, "width");
    ASSERT_FLOAT_EQ(r.height, 15.0f, 0.001f, "height");
}

/* ── areas ──────────────────────────────────────────────────────── */

static void test_rect_inter_area_half(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {5, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_rect_inter_area(a, b), 50.0f, 0.001f, "inter=50");
}

static void test_rect_union_area_half(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {5, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_rect_union_area(a, b), 150.0f, 0.001f, "union=150");
}

static void test_bound_area(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {20, 20, 10, 10};
    ASSERT_FLOAT_EQ(ml_rect_bound_area(a, b), 900.0f, 0.001f, "bound=900");
}

/* ── rect_center ────────────────────────────────────────────────── */

static void test_rect_center_basic(void)
{
    ml_rect_t r = {10, 10, 20, 20};
    ml_point_t c = ml_rect_center(r);
    ASSERT_FLOAT_EQ(c.x, 20.0f, 0.001f, "cx=20");
    ASSERT_FLOAT_EQ(c.y, 20.0f, 0.001f, "cy=20");
}

static void test_rect_center_zero(void)
{
    ml_rect_t r = {5, 5, 0, 0};
    ml_point_t c = ml_rect_center(r);
    ASSERT_FLOAT_EQ(c.x, 5.0f, 0.001f, "cx=5");
    ASSERT_FLOAT_EQ(c.y, 5.0f, 0.001f, "cy=5");
}

/* ── iou ────────────────────────────────────────────────────────── */

static void test_iou_perfect(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {0, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_iou(a, b), 1.0f, 0.001f, "iou=1.0");
}

static void test_iou_none(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {20, 20, 10, 10};
    ASSERT_FLOAT_EQ(ml_iou(a, b), 0.0f, 0.001f, "iou=0.0");
}

static void test_iou_half(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {5, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_iou(a, b), 0.33333f, 0.001f, "iou=0.333");
}

static void test_iou_zero_area(void)
{
    ml_rect_t a = {0, 0, 0, 0};
    ml_rect_t b = {0, 0, 0, 0};
    ASSERT_FLOAT_EQ(ml_iou(a, b), 0.0f, 0.001f, "iou=0.0");
}

/* ── iou_above ──────────────────────────────────────────────────── */

static void test_iou_above_true(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {0, 0, 10, 10};
    ASSERT_EQ(ml_iou_above(a, b, 0.5f), 1, "overlap > 0.5");
}

static void test_iou_above_false(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {20, 20, 10, 10};
    ASSERT_EQ(ml_iou_above(a, b, 0.5f), 0, "no overlap");
}

/* ── iou_small ──────────────────────────────────────────────────── */

static void test_iou_small_inside(void)
{
    ml_rect_t a = {0, 0, 20, 20};
    ml_rect_t b = {5, 5, 5, 5};
    ASSERT_FLOAT_EQ(ml_iou_small(a, b), 1.0f, 0.001f, "small=1.0");
}

static void test_iou_small_partial(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {5, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_iou_small(a, b), 0.5f, 0.001f, "small=0.5");
}

/* ── giou ───────────────────────────────────────────────────────── */

static void test_giou_perfect(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {0, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_giou(a, b), 1.0f, 0.001f, "giou=1.0");
}

static void test_giou_far(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {20, 20, 10, 10};
    ASSERT(ml_giou(a, b) < 0.0f, "giou negative for far-apart boxes");
}

/* ── diou ───────────────────────────────────────────────────────── */

static void test_diou_perfect(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {0, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_diou(a, b), 1.0f, 0.001f, "diou=1.0");
}

static void test_diou_far(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {100, 100, 10, 10};
    ASSERT(ml_diou(a, b) < 0.0f, "diou negative for far-apart boxes");
}

/* ── ciou ───────────────────────────────────────────────────────── */

static void test_ciou_perfect(void)
{
    ml_rect_t a = {0, 0, 10, 10};
    ml_rect_t b = {0, 0, 10, 10};
    ASSERT_FLOAT_EQ(ml_ciou(a, b), 1.0f, 0.001f, "ciou=1.0");
}

static void test_ciou_aspect_penalty(void)
{
    ml_rect_t a = {0, 0, 10, 5};
    ml_rect_t b = {0, 0, 5, 10};
    float v = ml_ciou(a, b);
    ASSERT(v < ml_iou(a, b), "ciou penalizes aspect ratio mismatch");
}

/* ── point_distance ─────────────────────────────────────────────── */

static void test_point_distance_zero(void)
{
    ml_point_t a = {0, 0};
    ml_point_t b = {0, 0};
    ASSERT_FLOAT_ZERO(ml_point_distance(a, b), "distance=0");
}

static void test_point_distance_345(void)
{
    ml_point_t a = {0, 0};
    ml_point_t b = {3, 4};
    ASSERT_FLOAT_EQ(ml_point_distance(a, b), 5.0f, 0.001f, "distance=5");
}

/* ── point_in_rect ──────────────────────────────────────────────── */

static void test_point_in_rect_inside(void)
{
    ml_rect_t  r = {10, 10, 20, 20};
    ml_point_t p = {15, 15};
    ASSERT_EQ(ml_point_in_rect(r, p), 1, "inside");
}

static void test_point_in_rect_edge(void)
{
    ml_rect_t  r = {10, 10, 20, 20};
    ml_point_t p = {10, 10};
    ASSERT_EQ(ml_point_in_rect(r, p), 1, "on left-top edge");
}

static void test_point_in_rect_outside(void)
{
    ml_rect_t  r = {10, 10, 20, 20};
    ml_point_t p = {0, 0};
    ASSERT_EQ(ml_point_in_rect(r, p), 0, "outside");
}

static void test_point_in_rect_boundary(void)
{
    ml_rect_t  r = {10, 10, 20, 20};
    ml_point_t p = {30, 30};
    ASSERT_EQ(ml_point_in_rect(r, p), 0, "exclusive right-bottom");
}

/* ── point_in_polygon ───────────────────────────────────────────── */

static void test_point_in_poly_inside_square(void)
{
    ml_point_t poly[4] = {{0,0},{10,0},{10,10},{0,10}};
    ASSERT_EQ(ml_point_in_polygon((ml_point_t){5,5}, poly, 4), 1, "inside square");
}

static void test_point_in_poly_outside_square(void)
{
    ml_point_t poly[4] = {{0,0},{10,0},{10,10},{0,10}};
    ASSERT_EQ(ml_point_in_polygon((ml_point_t){15,15}, poly, 4), 0, "outside square");
}

static void test_point_in_poly_inside_triangle(void)
{
    ml_point_t tri[3] = {{0,0},{10,0},{5,10}};
    ASSERT_EQ(ml_point_in_polygon((ml_point_t){5,5}, tri, 3), 1, "inside tri");
}

static void test_point_in_poly_outside_triangle(void)
{
    ml_point_t tri[3] = {{0,0},{10,0},{5,10}};
    ASSERT_EQ(ml_point_in_polygon((ml_point_t){0,10}, tri, 3), 0, "outside tri");
}

static void test_point_in_poly_null(void)
{
    ASSERT_EQ(ml_point_in_polygon((ml_point_t){0,0}, NULL, 0), 0, "null poly");
}

/* ── vec_norm ───────────────────────────────────────────────────── */

static void test_vec_norm_unit(void)
{
    float v[] = {1, 0, 0};
    ASSERT_FLOAT_EQ(ml_vec_norm(v, 3), 1.0f, 0.001f, "norm=1");
}

static void test_vec_norm_345(void)
{
    float v[] = {3, 4};
    ASSERT_FLOAT_EQ(ml_vec_norm(v, 2), 5.0f, 0.001f, "norm=5");
}

static void test_vec_norm_null(void)
{
    ASSERT_FLOAT_ZERO(ml_vec_norm(NULL, 0), "null=0");
}

/* ── cosine_similarity ──────────────────────────────────────────── */

static void test_cos_identical(void)
{
    float a[] = {1, 2, 3};
    float b[] = {1, 2, 3};
    ASSERT_FLOAT_EQ(ml_cosine_similarity(a, b, 3), 1.0f, 0.0001f, "cos=1");
}

static void test_cos_opposite(void)
{
    float a[] = {1, 0};
    float b[] = {-1, 0};
    ASSERT_FLOAT_EQ(ml_cosine_similarity(a, b, 2), -1.0f, 0.0001f, "cos=-1");
}

static void test_cos_orthogonal(void)
{
    float a[] = {1, 0};
    float b[] = {0, 1};
    ASSERT_FLOAT_EQ(ml_cosine_similarity(a, b, 2), 0.0f, 0.0001f, "cos=0");
}

static void test_cos_null(void)
{
    ASSERT_FLOAT_ZERO(ml_cosine_similarity(NULL, NULL, 0), "null=0");
}

/* ── euclidean_distance ─────────────────────────────────────────── */

static void test_euclid_same(void)
{
    float a[] = {1, 2, 3};
    float b[] = {1, 2, 3};
    ASSERT_FLOAT_ZERO(ml_euclidean_distance(a, b, 3), "dist=0");
}

static void test_euclid_345(void)
{
    float a[] = {0, 0};
    float b[] = {3, 4};
    ASSERT_FLOAT_EQ(ml_euclidean_distance(a, b, 2), 5.0f, 0.001f, "dist=5");
}

static void test_euclid_null(void)
{
    ASSERT_FLOAT_EQ(ml_euclidean_distance(NULL, NULL, 0), -1.0f, 0.001f, "null=-1");
}

/* ── combined scenario ──────────────────────────────────────────── */

static void test_best_match_by_iou(void)
{
    ml_rect_t gt    = {50, 50, 20, 20};
    ml_rect_t preds[] = {
        {52, 52, 18, 18},
        {0, 0, 10, 10},
        {100, 100, 20, 20},
    };
    int best = 0;
    float best_iou = ml_iou(gt, preds[0]);
    int i;
    for (i = 1; i < 3; i++) {
        float iou = ml_iou(gt, preds[i]);
        if (iou > best_iou) { best_iou = iou; best = i; }
    }
    ASSERT_EQ(best, 0, "best match is index 0");
    ASSERT(best_iou > 0.8f, "best iou > 0.8");
}

/* ── NMS ─────────────────────────────────────────────────────────── */

static void test_nms_single_box(void)
{
    ml_nms_box_t boxes[1] = {{0, 0, 10, 10, 0.9f, 0}};
    int n = ml_nms_multi_class(boxes, 1, 0.5f, &g_pool);
    ASSERT_EQ(n, 1, "single box kept");
}

static void test_nms_suppress_duplicate(void)
{
    ml_nms_box_t boxes[3] = {
        {0, 0, 10, 10, 0.9f, 0},
        {1, 1, 10, 10, 0.8f, 0},   /* high IoU → suppressed */
        {0, 0, 10, 10, 0.7f, 0},   /* high IoU → suppressed */
    };
    int n = ml_nms_multi_class(boxes, 3, 0.5f, &g_pool);
    ASSERT_EQ(n, 1, "only highest score kept");
}

static void test_nms_keep_different_class(void)
{
    ml_nms_box_t boxes[2] = {
        {0, 0, 10, 10, 0.9f, 0},
        {0, 0, 10, 10, 0.8f, 1},   /* same box, different class → kept */
    };
    int n = ml_nms_multi_class(boxes, 2, 0.5f, &g_pool);
    ASSERT_EQ(n, 2, "different classes both kept");
}

static void test_nms_keep_far(void)
{
    ml_nms_box_t boxes[2] = {
        {0,   0, 10, 10, 0.9f, 0},
        {100, 0, 10, 10, 0.8f, 0},  /* far away → kept */
    };
    int n = ml_nms_multi_class(boxes, 2, 0.5f, &g_pool);
    ASSERT_EQ(n, 2, "far boxes both kept");
}

static void test_nms_empty(void)
{
    ASSERT_EQ(ml_nms_multi_class(NULL, 0, 0.5f, &g_pool), 0, "empty → 0");
}

static void test_nms_no_suppress(void)
{
    ml_nms_box_t boxes[3] = {
        {0,   0, 10, 10, 0.9f, 0},
        {20,  0, 10, 10, 0.8f, 0},
        {100, 0, 10, 10, 0.7f, 0},
    };
    int n = ml_nms_multi_class(boxes, 3, 0.5f, &g_pool);
    ASSERT_EQ(n, 3, "no overlap → all kept");
}

/* ── Kalman1D ─────────────────────────────────────────────────────── */

static void test_kalman_init(void)
{
    ml_kalman1d_t kf;
    ml_kalman1d_init(&kf, 0.1f, 0.5f, 10.0f, 1.0f);
    ASSERT_FLOAT_EQ(kf.x, 10.0f, 0.001f, "init x");
    ASSERT_FLOAT_EQ(kf.v, 0.0f, 0.001f, "init v=0");
}

static void test_kalman_null_safe(void)
{
    ml_kalman1d_init(NULL, 0.1f, 0.5f, 0.0f, 0.0f);
    ml_kalman1d_predict(NULL, 1.0f);
    ml_kalman1d_update(NULL, 5.0f);
    ASSERT_FLOAT_EQ(ml_kalman1d_estimate(NULL), 0.0f, 0.001f, "null estimate=0");
    ASSERT_FLOAT_EQ(ml_kalman1d_velocity(NULL), 0.0f, 0.001f, "null velocity=0");
    /* all should be no-ops without crash */
}

static void test_kalman_predict(void)
{
    ml_kalman1d_t kf;
    ml_kalman1d_init(&kf, 0.0f, 1.0f, 10.0f, 1.0f);
    ml_kalman1d_predict(&kf, 1.0f);
    ASSERT_FLOAT_EQ(ml_kalman1d_estimate(&kf), 10.0f, 0.001f, "predict x unchanged (v=0)");
}

static void test_kalman_update_converges(void)
{
    ml_kalman1d_t kf;
    ml_kalman1d_init(&kf, 0.01f, 0.1f, 0.0f, 10.0f);
    int i;
    for (i = 0; i < 20; i++) {
        ml_kalman1d_predict(&kf, 1.0f);
        ml_kalman1d_update(&kf, 5.0f);
    }
    ASSERT(ml_kalman1d_estimate(&kf) > 4.0f, "converges toward 5");
    ASSERT(ml_kalman1d_estimate(&kf) < 6.0f, "within range");
}

static void test_kalman_velocity_tracks(void)
{
    ml_kalman1d_t kf;
    ml_kalman1d_init(&kf, 0.01f, 0.5f, 0.0f, 10.0f);
    int i;
    /* simulate constant velocity: measurement increases by 2 per step */
    for (i = 0; i < 30; i++) {
        ml_kalman1d_predict(&kf, 1.0f);
        ml_kalman1d_update(&kf, 2.0f * (float)(i + 1));
    }
    ASSERT(ml_kalman1d_velocity(&kf) > 0.5f, "velocity positive");
}

static void test_kalman_reset(void)
{
    ml_kalman1d_t kf;
    ml_kalman1d_init(&kf, 0.1f, 0.5f, 10.0f, 1.0f);
    ml_kalman1d_predict(&kf, 1.0f);
    ml_kalman1d_update(&kf, 20.0f);
    ml_kalman1d_reset(&kf, 5.0f);
    ASSERT_FLOAT_EQ(ml_kalman1d_estimate(&kf), 5.0f, 0.001f, "reset to 5");
    ASSERT_FLOAT_EQ(ml_kalman1d_velocity(&kf), 0.0f, 0.001f, "reset v=0");
}

/* ── letterbox ────────────────────────────────────────────────────── */

static void test_letterbox_identity(void)
{
    float x = 320.0f, y = 240.0f;
    ml_letterbox_to_source(&x, &y, 640, 480, 640, 480);
    ASSERT_FLOAT_EQ(x, 320.0f, 0.01f, "x unchanged");
    ASSERT_FLOAT_EQ(y, 240.0f, 0.01f, "y unchanged");
}

static void test_letterbox_square_pad(void)
{
    /* 640×480 → 640×640: letterbox pads top/bottom 80px each */
    /* model y=120 → source y = (120 - 80) / 1.0 = 40 */
    float x = 320.0f, y = 120.0f;
    ml_letterbox_to_source(&x, &y, 640, 480, 640, 640);
    ASSERT_FLOAT_EQ(x, 320.0f, 0.01f, "x unchanged");
    ASSERT_FLOAT_EQ(y, 40.0f, 0.01f, "y=40 after removing top pad");
}

static void test_letterbox_null_ptr(void)
{
    float x = 1.0f;
    ml_letterbox_to_source(NULL, &x, 640, 480, 640, 640);
    ml_letterbox_to_source(&x, NULL, 640, 480, 640, 640);
    /* should be no-ops without crash */
}

static void test_letterbox_zero_dims(void)
{
    float x = 100.0f, y = 100.0f;
    float orig_x = x, orig_y = y;
    ml_letterbox_to_source(&x, &y, 0, 480, 640, 640);  /* src_w=0 */
    ASSERT_FLOAT_EQ(x, orig_x, 0.001f, "unchanged on zero src_w");
    ml_letterbox_to_source(&x, &y, 640, 0, 640, 640);  /* src_h=0 */
    ml_letterbox_to_source(&x, &y, 640, 480, 0, 640);  /* model_w=0 */
    ml_letterbox_to_source(&x, &y, 640, 480, 640, 0);  /* model_h=0 */
}

/* ── clamp ────────────────────────────────────────────────────────── */

static void test_clamp_noop(void)
{
    ml_rect_t r = {10, 10, 20, 20};
    ml_rect_t c = ml_rect_clamp(r, 640.0f, 480.0f);
    ASSERT_FLOAT_EQ(c.x, 10.0f, 0.001f, "x unchanged");
    ASSERT_FLOAT_EQ(c.width, 20.0f, 0.001f, "w unchanged");
}

static void test_clamp_negative_x(void)
{
    ml_rect_t r = {-5, 10, 20, 20};
    ml_rect_t c = ml_rect_clamp(r, 640.0f, 480.0f);
    ASSERT_FLOAT_EQ(c.x, 0.0f, 0.001f, "x clamped to 0");
    ASSERT_FLOAT_EQ(c.width, 15.0f, 0.001f, "width shrunk by offset");
}

static void test_clamp_exceeds_image(void)
{
    ml_rect_t r = {630, 470, 20, 20};
    ml_rect_t c = ml_rect_clamp(r, 640.0f, 480.0f);
    ASSERT_FLOAT_EQ(c.x, 630.0f, 0.001f, "x unchanged");
    ASSERT_FLOAT_EQ(c.width, 10.0f, 0.001f, "width clamped to img edge");
    ASSERT_FLOAT_EQ(c.height, 10.0f, 0.001f, "height clamped to img edge");
}

static void test_clamp_zero_size(void)
{
    ml_rect_t r = {-10, -10, 5, 5};
    ml_rect_t c = ml_rect_clamp(r, 640.0f, 480.0f);
    ASSERT_FLOAT_EQ(c.x, 0.0f, 0.001f, "negative x → 0");
    ASSERT_FLOAT_EQ(c.width, 0.0f, 0.001f, "width clipped to 0");
}

/* ── suite ──────────────────────────────────────────────────────── */

void test_cv_math_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_cv_math");

    RUN_TEST("rect_inter - overlapping",   test_rect_inter_overlap());
    RUN_TEST("rect_inter - no overlap",    test_rect_inter_no_overlap());
    RUN_TEST("rect_inter - contained",     test_rect_inter_contained());
    RUN_TEST("rect_union - overlapping",   test_rect_union_overlap());
    RUN_TEST("rect_inter_area - half",     test_rect_inter_area_half());
    RUN_TEST("rect_union_area - half",     test_rect_union_area_half());
    RUN_TEST("bound_area",                 test_bound_area());
    RUN_TEST("rect_center - basic",        test_rect_center_basic());
    RUN_TEST("rect_center - zero size",    test_rect_center_zero());

    RUN_TEST("ml_iou - perfect",           test_iou_perfect());
    RUN_TEST("ml_iou - no overlap",        test_iou_none());
    RUN_TEST("ml_iou - half overlap",      test_iou_half());
    RUN_TEST("ml_iou - zero area",         test_iou_zero_area());
    RUN_TEST("ml_iou_above - true",        test_iou_above_true());
    RUN_TEST("ml_iou_above - false",       test_iou_above_false());

    RUN_TEST("ml_iou_small - inside",      test_iou_small_inside());
    RUN_TEST("ml_iou_small - partial",     test_iou_small_partial());

    RUN_TEST("ml_giou - perfect",          test_giou_perfect());
    RUN_TEST("ml_giou - far apart",        test_giou_far());

    RUN_TEST("ml_diou - perfect",          test_diou_perfect());
    RUN_TEST("ml_diou - far apart",        test_diou_far());

    RUN_TEST("ml_ciou - perfect",          test_ciou_perfect());
    RUN_TEST("ml_ciou - aspect penalty",   test_ciou_aspect_penalty());

    RUN_TEST("point_distance - zero",      test_point_distance_zero());
    RUN_TEST("point_distance - 3-4-5",     test_point_distance_345());

    RUN_TEST("point_in_rect - inside",     test_point_in_rect_inside());
    RUN_TEST("point_in_rect - edge",       test_point_in_rect_edge());
    RUN_TEST("point_in_rect - outside",    test_point_in_rect_outside());
    RUN_TEST("point_in_rect - boundary",   test_point_in_rect_boundary());

    RUN_TEST("point_in_polygon - inside",  test_point_in_poly_inside_square());
    RUN_TEST("point_in_polygon - outside", test_point_in_poly_outside_square());
    RUN_TEST("point_in_polygon - tri in",  test_point_in_poly_inside_triangle());
    RUN_TEST("point_in_polygon - tri out", test_point_in_poly_outside_triangle());
    RUN_TEST("point_in_polygon - null",    test_point_in_poly_null());

    RUN_TEST("vec_norm - unit",            test_vec_norm_unit());
    RUN_TEST("vec_norm - 3-4-5",           test_vec_norm_345());
    RUN_TEST("vec_norm - null",            test_vec_norm_null());

    RUN_TEST("cosine_similarity - id",     test_cos_identical());
    RUN_TEST("cosine_similarity - opp",    test_cos_opposite());
    RUN_TEST("cosine_similarity - orth",   test_cos_orthogonal());
    RUN_TEST("cosine_similarity - null",   test_cos_null());

    RUN_TEST("euclidean_distance - same",  test_euclid_same());
    RUN_TEST("euclidean_distance - 3-4-5", test_euclid_345());
    RUN_TEST("euclidean_distance - null",  test_euclid_null());

    RUN_TEST("scenario - best iou match",  test_best_match_by_iou());

    RUN_TEST("nms - single box",           test_nms_single_box());
    RUN_TEST("nms - suppress duplicate",   test_nms_suppress_duplicate());
    RUN_TEST("nms - keep different class", test_nms_keep_different_class());
    RUN_TEST("nms - keep far boxes",       test_nms_keep_far());
    RUN_TEST("nms - empty input",          test_nms_empty());
    RUN_TEST("nms - no suppression",       test_nms_no_suppress());

    RUN_TEST("kalman - init",              test_kalman_init());
    RUN_TEST("kalman - null safe",         test_kalman_null_safe());
    RUN_TEST("kalman - predict",           test_kalman_predict());
    RUN_TEST("kalman - update converges",  test_kalman_update_converges());
    RUN_TEST("kalman - velocity tracks",   test_kalman_velocity_tracks());
    RUN_TEST("kalman - reset",             test_kalman_reset());

    RUN_TEST("letterbox - identity",       test_letterbox_identity());
    RUN_TEST("letterbox - square pad",     test_letterbox_square_pad());
    RUN_TEST("letterbox - null ptr",       test_letterbox_null_ptr());
    RUN_TEST("letterbox - zero dims",      test_letterbox_zero_dims());

    RUN_TEST("clamp - no-op",              test_clamp_noop());
    RUN_TEST("clamp - negative x",         test_clamp_negative_x());
    RUN_TEST("clamp - exceeds image",      test_clamp_exceeds_image());
    RUN_TEST("clamp - zero size",          test_clamp_zero_size());

    ml_pool_destroy(&g_pool);
}
