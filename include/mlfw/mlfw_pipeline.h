/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * mlfw_pipeline — JSON-driven multimedia pipeline.
 *
 * Chains camera → preprocess → inference → encode → output stages,
 * each running in its own thread with bounded queues between them.
 *
 * Usage:
 *   mlfw_pipeline_t *p = mlfw_pipeline_create(json_cfg, &pool);
 *   mlfw_pipeline_start(p);
 *   // ... read stats ...
 *   mlfw_pipeline_stop(p);
 *   mlfw_pipeline_destroy(p);
 */

#ifndef MLFW_PIPELINE_H
#define MLFW_PIPELINE_H

#include "ml_pool.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── stage types ──────────────────────────────────────────────────── */

#define MLFW_PIPELINE_MAX_STAGES  8

typedef enum {
    MLFW_STAGE_SOURCE = 0,
    MLFW_STAGE_PREPROCESS,
    MLFW_STAGE_INFER,
    MLFW_STAGE_ENCODE,
    MLFW_STAGE_OUTPUT,
} mlfw_stage_type_t;

typedef enum {
    MLFW_STAGE_STOPPED = 0,
    MLFW_STAGE_RUNNING,
    MLFW_STAGE_ERROR,
} mlfw_stage_state_t;

/* ── per-stage stats ──────────────────────────────────────────────── */

typedef struct {
    mlfw_stage_type_t  type;
    mlfw_stage_state_t state;
    const char        *name;

    double   fps;
    double   avg_latency_ms;
    double   max_latency_ms;
    uint64_t frames_processed;
    uint64_t frames_dropped;
    uint64_t errors;
    uint64_t last_frame_ts_us;
} mlfw_stage_stats_t;

/* ── pipeline stats ───────────────────────────────────────────────── */

typedef struct {
    int      num_stages;
    int      uptime_sec;
    double   total_fps;
    mlfw_stage_stats_t stages[MLFW_PIPELINE_MAX_STAGES];
} mlfw_pipeline_stats_t;

/* ── opaque handle ────────────────────────────────────────────────── */

typedef struct mlfw_pipeline_s mlfw_pipeline_t;

/* ── lifecycle ────────────────────────────────────────────────────── */

/* Create a pipeline from JSON config. pool is used for internal allocations.
 *
 * JSON format:
 * {
 *   "source":      { "type": "v4l2", "device": "/dev/video0" }
 *                 | { "type": "rtsp", "url": "rtsp://..." }
 *                 | { "type": "image", "path": "test.jpg" }     (oneshot)
 *                 | { "type": "video", "path": "test.mp4" },
 *
 *   "preprocess":  { "width": 640, "height": 640 },
 *
 *   "infer":       { "model": "yolov5nu.onnx", "backend": "onnx",
 *                    "conf": 0.25, "nms": 0.45 },
 *
 *   "encode":      { "codec": "h264", "bitrate": 2000000 },
 *
 *   "output":      { "type": "rtsp", "mount": "/stream" }
 *                | { "type": "mp4",  "path": "output.mp4" }
 *                | { "type": "ws",   "port": 9000 }
 * }
 *
 * All stages are optional. Omitted stages are simply not created.
 */
mlfw_pipeline_t *mlfw_pipeline_create(const char *json_cfg, ml_pool_t *pool);

/* Destroy the pipeline. Stops all threads and frees resources. */
void             mlfw_pipeline_destroy(mlfw_pipeline_t *p);

/* ── control ──────────────────────────────────────────────────────── */

int  mlfw_pipeline_start(mlfw_pipeline_t *p);
int  mlfw_pipeline_stop(mlfw_pipeline_t *p);
int  mlfw_pipeline_is_running(mlfw_pipeline_t *p);

/* ── query ────────────────────────────────────────────────────────── */

int         mlfw_pipeline_get_stats(mlfw_pipeline_t *p, mlfw_pipeline_stats_t *out);
const char *mlfw_pipeline_error(mlfw_pipeline_t *p);

#ifdef __cplusplus
}
#endif

#endif /* MLFW_PIPELINE_H */
