/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_mux — FFmpeg-based video muxer.
 *
 * Wraps libavformat to mux H.264/H.265 elementary streams into
 * MP4 / FLV / MKV containers.  Stateless per-file API:
 *   create → set_video → write_header → write_frame* → destroy
 */

#ifndef __ML_MUX_H__
#define __ML_MUX_H__

#include <stdint.h>

typedef struct ml_mux_s ml_mux_t;

/* ── enums ──────────────────────────────────────────────────────────── */

typedef enum {
    ML_MUX_FMT_MP4 = 0,
    ML_MUX_FMT_FLV,
    ML_MUX_FMT_MKV,
} ml_mux_format_e;

typedef enum {
    ML_MUX_CODEC_H264 = 0,
    ML_MUX_CODEC_H265,
} ml_mux_codec_e;

/* ── lifecycle ──────────────────────────────────────────────────────── */

/* Create muxer writing to `filename` in the given container format.
 * Returns NULL on failure (bad path, unsupported format). */
ml_mux_t *ml_mux_create(const char *filename, ml_mux_format_e format);

/* Configure the video stream.  Must be called before ml_mux_write_header.
 *   codec:       H264 or H265
 *   width, height: pixel dimensions
 *   fps_num/den: framerate fraction (e.g. 30/1 for 30 fps)
 *   extradata:   codec-private data (SPS+PPS for H.264, VPS+SPS+PPS for H.265)
 *   extradata_len: size of extradata in bytes
 * Returns 0 on success, -1 on error. */
int ml_mux_set_video(ml_mux_t *m, ml_mux_codec_e codec,
                     int width, int height,
                     int fps_num, int fps_den,
                     const uint8_t *extradata, int extradata_len);

/* Write container header.  Must be called after ml_mux_set_video.
 * Returns 0 on success, -1 on error. */
int ml_mux_write_header(ml_mux_t *m);

/* Write one video frame.
 *   data, len:  encoded frame data (NAL unit)
 *   pts_ms:     presentation timestamp in milliseconds
 *   keyframe:   1 if this is an IDR / keyframe, 0 otherwise
 * Returns 0 on success, -1 on error. */
int ml_mux_write_frame(ml_mux_t *m, const uint8_t *data, int len,
                       int64_t pts_ms, int keyframe);

/* Write trailer and close the output file.  Safe to call on NULL. */
void ml_mux_destroy(ml_mux_t *m);

#endif /* __ML_MUX_H__ */
