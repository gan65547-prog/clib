/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_cv_math — Lightweight CV math utilities (IoU, geometry, feature vectors).
 *
 * Pure C11, zero dependencies beyond libm.  Float precision throughout
 * because detection-box coordinates from RKNN/ONNX are naturally float.
 */

#ifndef __ML_CV_MATH_H__
#define __ML_CV_MATH_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ── types ──────────────────────────────────────────────────────────── */

typedef struct {
    float x, y;
} ml_point_t;

typedef struct {
    float x, y;
    float width, height;
} ml_rect_t;

/* ── IoU series ─────────────────────────────────────────────────────── */

float ml_iou(ml_rect_t a, ml_rect_t b);
float ml_giou(ml_rect_t a, ml_rect_t b);
float ml_diou(ml_rect_t a, ml_rect_t b);
float ml_ciou(ml_rect_t a, ml_rect_t b);

/* variant: intersection over the *smaller* box area (not union) */
float ml_iou_small(ml_rect_t a, ml_rect_t b);

/* convenience: returns 1 when ml_iou(a,b) > threshold */
int   ml_iou_above(ml_rect_t a, ml_rect_t b, float threshold);

/* ── rectangle geometry ─────────────────────────────────────────────── */

ml_rect_t  ml_rect_inter(ml_rect_t a, ml_rect_t b);
ml_rect_t  ml_rect_union(ml_rect_t a, ml_rect_t b);   /* tight bounding box of both */
float      ml_rect_inter_area(ml_rect_t a, ml_rect_t b);
float      ml_rect_union_area(ml_rect_t a, ml_rect_t b);
float      ml_rect_bound_area(ml_rect_t a, ml_rect_t b);
ml_point_t ml_rect_center(ml_rect_t r);
float      ml_rect_center_distance(ml_rect_t a, ml_rect_t b);
float      ml_rect_bound_diagonal(ml_rect_t a, ml_rect_t b);

/* ── point utilities ────────────────────────────────────────────────── */

float ml_point_distance(ml_point_t a, ml_point_t b);
int   ml_point_in_rect(ml_rect_t r, ml_point_t p);
int   ml_point_in_polygon(ml_point_t p, const ml_point_t *poly, int n);

/* ── feature-vector math ────────────────────────────────────────────── */

float ml_vec_norm(const float *v, int n);
float ml_cosine_similarity(const float *a, const float *b, int n);
float ml_euclidean_distance(const float *a, const float *b, int n);

#ifdef __cplusplus
}
#endif

#endif /* __ML_CV_MATH_H__ */
