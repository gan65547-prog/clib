/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include "ml_cv_math.h"
#include <math.h>
#include <float.h>

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
