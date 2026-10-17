/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_cv_math.h"
#include "ml_pool.h"
#include <math.h>
#include <float.h>
#include <string.h>

/* ── internal helpers ───────────────────────────────────────────────── */

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }

/* ── rectangle intersection / union / bound ─────────────────────────── */

ml_rect_t
ml_rect_inter(ml_rect_t a, ml_rect_t b)
{
    ml_rect_t r;
    float ax2 = a.x + a.width, ay2 = a.y + a.height;
    float bx2 = b.x + b.width, by2 = b.y + b.height;

    r.x      = maxf(a.x, b.x);
    r.y      = maxf(a.y, b.y);
    r.width  = maxf(minf(ax2, bx2) - r.x, 0.0f);
    r.height = maxf(minf(ay2, by2) - r.y, 0.0f);
    return r;
}

ml_rect_t
ml_rect_union(ml_rect_t a, ml_rect_t b)
{
    ml_rect_t r;
    float ax2 = a.x + a.width, ay2 = a.y + a.height;
    float bx2 = b.x + b.width, by2 = b.y + b.height;

    r.x      = minf(a.x, b.x);
    r.y      = minf(a.y, b.y);
    r.width  = maxf(ax2, bx2) - r.x;
    r.height = maxf(ay2, by2) - r.y;
    return r;
}

float
ml_rect_inter_area(ml_rect_t a, ml_rect_t b)
{
    ml_rect_t r = ml_rect_inter(a, b);
    return r.width * r.height;
}

float
ml_rect_union_area(ml_rect_t a, ml_rect_t b)
{
    float area_a = a.width * a.height;
    float area_b = b.width * b.height;
    float inter  = ml_rect_inter_area(a, b);
    return area_a + area_b - inter;
}

float
ml_rect_bound_area(ml_rect_t a, ml_rect_t b)
{
    ml_rect_t r = ml_rect_union(a, b);
    return r.width * r.height;
}

ml_point_t
ml_rect_center(ml_rect_t r)
{
    ml_point_t p;
    p.x = r.x + r.width  * 0.5f;
    p.y = r.y + r.height * 0.5f;
    return p;
}

float
ml_rect_center_distance(ml_rect_t a, ml_rect_t b)
{
    ml_point_t ca = ml_rect_center(a);
    ml_point_t cb = ml_rect_center(b);
    return ml_point_distance(ca, cb);
}

float
ml_rect_bound_diagonal(ml_rect_t a, ml_rect_t b)
{
    ml_rect_t r = ml_rect_union(a, b);
    return sqrtf(r.width * r.width + r.height * r.height);
}

/* ── IoU series ─────────────────────────────────────────────────────── */

float
ml_iou(ml_rect_t a, ml_rect_t b)
{
    float inter = ml_rect_inter_area(a, b);
    float uni   = ml_rect_union_area(a, b);
    if (uni < FLT_EPSILON) return 0.0f;
    return inter / uni;
}

float
ml_iou_small(ml_rect_t a, ml_rect_t b)
{
    float inter   = ml_rect_inter_area(a, b);
    float area_a  = a.width * a.height;
    float area_b  = b.width * b.height;
    float smaller = area_a < area_b ? area_a : area_b;
    if (smaller < FLT_EPSILON) return 0.0f;
    return inter / smaller;
}

int
ml_iou_above(ml_rect_t a, ml_rect_t b, float threshold)
{
    return ml_iou(a, b) > threshold ? 1 : 0;
}

float
ml_giou(ml_rect_t a, ml_rect_t b)
{
    float inter = ml_rect_inter_area(a, b);
    float uni   = ml_rect_union_area(a, b);
    float bound = ml_rect_bound_area(a, b);

    if (bound < FLT_EPSILON) return 0.0f;
    if (uni   < FLT_EPSILON) return 0.0f;

    return (inter / uni) - ((bound - uni) / bound);
}

float
ml_diou(ml_rect_t a, ml_rect_t b)
{
    float iou  = ml_iou(a, b);
    float cd   = ml_rect_center_distance(a, b);
    float diag = ml_rect_bound_diagonal(a, b);

    if (diag < FLT_EPSILON) return iou;
    return iou - (cd * cd) / (diag * diag);
}

float
ml_ciou(ml_rect_t a, ml_rect_t b)
{
    float iou = ml_iou(a, b);

    /* aspect-ratio penalty v */
    float ar_a = (a.height > FLT_EPSILON) ? atanf(a.width / a.height) : 0.0f;
    float ar_b = (b.height > FLT_EPSILON) ? atanf(b.width / b.height) : 0.0f;
    float v    = (4.0f / (float)(M_PI * M_PI)) * (ar_a - ar_b) * (ar_a - ar_b);

    /* trade-off parameter alpha */
    float alpha = 0.0f;
    float denom = 1.0f - iou + v;
    if (fabsf(denom) > FLT_EPSILON)
        alpha = v / denom;

    return ml_diou(a, b) - alpha * v;
}

/* ── point utilities ────────────────────────────────────────────────── */

float
ml_point_distance(ml_point_t a, ml_point_t b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

int
ml_point_in_rect(ml_rect_t r, ml_point_t p)
{
    return (p.x >= r.x && p.x < r.x + r.width &&
            p.y >= r.y && p.y < r.y + r.height) ? 1 : 0;
}

int
ml_point_in_polygon(ml_point_t p, const ml_point_t *poly, int n)
{
    int i, j, inside = 0;

    if (!poly || n < 3) return 0;

    for (i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
            (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) /
                   (poly[j].y - poly[i].y) + poly[i].x))
        {
            inside = !inside;
        }
    }

    return inside;
}

/* ── feature-vector math ────────────────────────────────────────────── */

float
ml_vec_norm(const float *v, int n)
{
    float sum = 0.0f;
    int i;
    if (!v || n <= 0) return 0.0f;
    for (i = 0; i < n; i++)
        sum += v[i] * v[i];
    return sqrtf(sum);
}

float
ml_cosine_similarity(const float *a, const float *b, int n)
{
    float dot = 0.0f;
    float norm_a, norm_b, denom;
    int i;

    if (!a || !b || n <= 0) return 0.0f;

    for (i = 0; i < n; i++)
        dot += a[i] * b[i];

    norm_a = ml_vec_norm(a, n);
    norm_b = ml_vec_norm(b, n);
    denom  = norm_a * norm_b;

    if (denom < FLT_EPSILON) return 0.0f;
    return dot / denom;
}

float
ml_euclidean_distance(const float *a, const float *b, int n)
{
    float sum = 0.0f, diff;
    int i;

    if (!a || !b || n <= 0) return -1.0f;

    for (i = 0; i < n; i++) {
        diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/* ── NMS (non-maximum suppression) ─────────────────────────────────── */

static int nms_score_desc(const void *a, const void *b)
{
    float sa = ((const ml_nms_box_t *)a)->score;
    float sb = ((const ml_nms_box_t *)b)->score;
    return (sa < sb) ? 1 : ((sa > sb) ? -1 : 0);
}

int
ml_nms_multi_class(ml_nms_box_t *boxes, int count, float iou_threshold, ml_pool_t *pool)
{
    int i, j, keep_count;

    if (!boxes || count <= 1 || !pool) return count;

    /* sort by score descending */
    qsort(boxes, (size_t)count, sizeof(ml_nms_box_t), nms_score_desc);

    /* allocate keep flags on stack for small N; heap for safety */
    char  stack_buf[256];
    char *keep;
    int   use_heap = (count > 256);

    keep = use_heap ? (char *)pool->f_alloc(pool, (size_t)count) : stack_buf;
    if (!keep) return count;
    if (!use_heap) memset(stack_buf, 1, (size_t)count);
    else           memset(keep, 1, (size_t)count);

    for (i = 0; i < count; i++) {
        ml_rect_t a;
        if (!keep[i]) continue;
        a.x = boxes[i].x; a.y = boxes[i].y;
        a.width = boxes[i].width; a.height = boxes[i].height;

        for (j = i + 1; j < count; j++) {
            ml_rect_t b;
            if (!keep[j]) continue;
            if (boxes[i].class_id != boxes[j].class_id) continue;

            b.x = boxes[j].x; b.y = boxes[j].y;
            b.width = boxes[j].width; b.height = boxes[j].height;

            if (ml_iou(a, b) > iou_threshold)
                keep[j] = 0;
        }
    }

    /* compact — move kept boxes to front */
    keep_count = 0;
    for (i = 0; i < count; i++) {
        if (keep[i]) {
            if (keep_count != i)
                boxes[keep_count] = boxes[i];
            keep_count++;
        }
    }

    if (use_heap) pool->f_free(pool, keep);
    return keep_count;
}

/* ── coordinate transform ──────────────────────────────────────────── */

void
ml_letterbox_to_source(float *x, float *y,
                       int src_w, int src_h,
                       int model_w, int model_h)
{
    float scale, pad_x, pad_y;

    if (!x || !y) return;
    if (src_w <= 0 || src_h <= 0 || model_w <= 0 || model_h <= 0) return;

    scale = (float)model_w / (float)src_w;
    {
        float scale_h = (float)model_h / (float)src_h;
        if (scale_h < scale) scale = scale_h;
    }

    pad_x = ((float)model_w - (float)src_w * scale) * 0.5f;
    pad_y = ((float)model_h - (float)src_h * scale) * 0.5f;

    *x = (*x - pad_x) / scale;
    *y = (*y - pad_y) / scale;
}

ml_rect_t
ml_rect_clamp(ml_rect_t r, float img_w, float img_h)
{
    if (r.x < 0.0f)      { r.width += r.x; r.x = 0.0f; }
    if (r.y < 0.0f)      { r.height += r.y; r.y = 0.0f; }
    if (r.x + r.width  > img_w) r.width  = img_w - r.x;
    if (r.y + r.height > img_h) r.height = img_h - r.y;
    if (r.width  < 0.0f) r.width  = 0.0f;
    if (r.height < 0.0f) r.height = 0.0f;
    return r;
}

/* ── 1D Kalman filter (constant velocity, simplified) ──────────────────
 *
 * Simplified Kalman: tracks position (p11) and velocity (p22) covariance
 * independently.  No cross-covariance term (p12) — valid when position
 * and velocity errors are uncorrelated or when simplicity is preferred
 * over optimality.  Sufficient for trajectory smoothing; not for
 * sensor-fusion applications requiring full error propagation.
 */

void
ml_kalman1d_init(ml_kalman1d_t *kf, float process_noise,
                 float measure_noise, float init_est, float init_err)
{
    if (!kf) return;
    kf->x   = init_est;
    kf->v   = 0.0f;
    kf->p11 = init_err;
    kf->p22 = init_err;
    kf->q   = process_noise;
    kf->r   = measure_noise;
}

void
ml_kalman1d_predict(ml_kalman1d_t *kf, float dt)
{
    if (!kf) return;
    kf->x = kf->x + kf->v * dt;
    kf->p11 = kf->p11 + 2.0f * dt * kf->p11 + dt * dt * kf->p22 + kf->q;
    kf->p22 = kf->p22 + kf->q;
}

void
ml_kalman1d_update(ml_kalman1d_t *kf, float measurement)
{
    float k, innovation;
    if (!kf) return;
    k = kf->p11 / (kf->p11 + kf->r);
    innovation = measurement - kf->x;
    kf->x = kf->x + k * innovation;
    kf->v = kf->v + k * innovation;
    kf->p11 = (1.0f - k) * kf->p11;
    kf->p22 = (1.0f - k * 0.5f) * kf->p22;
}

float
ml_kalman1d_estimate(const ml_kalman1d_t *kf)
{
    return kf ? kf->x : 0.0f;
}

float
ml_kalman1d_velocity(const ml_kalman1d_t *kf)
{
    return kf ? kf->v : 0.0f;
}

void
ml_kalman1d_reset(ml_kalman1d_t *kf, float estimate)
{
    if (!kf) return;
    kf->x   = estimate;
    kf->v   = 0.0f;
    kf->p11 = 1.0f;
    kf->p22 = 1.0f;
}
