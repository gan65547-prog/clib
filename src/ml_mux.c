/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_mux.c — FFmpeg libavformat muxer for H.264/H.265 → MP4/FLV/MKV.
 */

#include "ml_mux.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include <stdlib.h>
#include <string.h>

struct ml_mux_s {
    AVFormatContext *ctx;
    AVStream        *stream;
    int              fps_num;
    int              fps_den;
    int              header_written;
    int64_t          last_dts;    /* for monotonic DTS tracking */
};

/* ── helpers ────────────────────────────────────────────────────────── */

static const char *
format_name(ml_mux_format_e fmt)
{
    switch (fmt) {
    case ML_MUX_FMT_MP4: return "mp4";
    case ML_MUX_FMT_FLV: return "flv";
    case ML_MUX_FMT_MKV: return "matroska";
    default:             return NULL;
    }
}

static enum AVCodecID
codec_id(ml_mux_codec_e c)
{
    switch (c) {
    case ML_MUX_CODEC_H264: return AV_CODEC_ID_H264;
    case ML_MUX_CODEC_H265: return AV_CODEC_ID_HEVC;
    default:                return AV_CODEC_ID_NONE;
    }
}

/* ── lifecycle ──────────────────────────────────────────────────────── */

ml_mux_t *
ml_mux_create(const char *filename, ml_mux_format_e fmt)
{
    ml_mux_t *m;
    const char *fname;
    const AVOutputFormat *ofmt;
    int ret;

    if (!filename) return NULL;

    fname = format_name(fmt);
    if (!fname) return NULL;

    ofmt = av_guess_format(fname, NULL, NULL);
    if (!ofmt) return NULL;

    m = calloc(1, sizeof(*m));
    if (!m) return NULL;

    ret = avformat_alloc_output_context2(&m->ctx, ofmt, NULL, filename);
    if (ret < 0 || !m->ctx) {
        free(m);
        return NULL;
    }

    return m;
}

/* ── configuration ──────────────────────────────────────────────────── */

int
ml_mux_set_video(ml_mux_t *m, ml_mux_codec_e codec,
                 int width, int height,
                 int fps_num, int fps_den,
                 const uint8_t *extradata, int extradata_len)
{
    AVCodecParameters *par;
    enum AVCodecID cid;

    if (!m || m->header_written || fps_num < 1 || fps_den < 1)
        return -1;

    cid = codec_id(codec);
    if (cid == AV_CODEC_ID_NONE) return -1;

    m->stream = avformat_new_stream(m->ctx, NULL);
    if (!m->stream) return -1;

    par = m->stream->codecpar;
    par->codec_type = AVMEDIA_TYPE_VIDEO;
    par->codec_id   = cid;
    par->width      = width;
    par->height     = height;

    m->fps_num = fps_num;
    m->fps_den = fps_den;

    /* timebase: 1/fps for frame-level granularity */
    m->stream->time_base = (AVRational){ fps_den, fps_num };

    /* extradata: SPS+PPS (H.264) or VPS+SPS+PPS (H.265) */
    if (extradata && extradata_len > 0) {
        par->extradata = av_mallocz(extradata_len + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!par->extradata) return -1;
        memcpy(par->extradata, extradata, extradata_len);
        par->extradata_size = extradata_len;
    }

    return 0;
}

/* ── write header ───────────────────────────────────────────────────── */

int
ml_mux_write_header(ml_mux_t *m)
{
    int ret;

    if (!m || !m->stream || m->header_written) return -1;

    /* open output */
    if (!(m->ctx->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m->ctx->pb, m->ctx->url, AVIO_FLAG_WRITE);
        if (ret < 0) return -1;
    }

    ret = avformat_write_header(m->ctx, NULL);
    if (ret < 0) return -1;

    m->header_written = 1;
    m->last_dts = -1;
    return 0;
}

/* ── write frame ────────────────────────────────────────────────────── */

int
ml_mux_write_frame(ml_mux_t *m, const uint8_t *data, int len,
                   int64_t pts_ms, int keyframe)
{
    AVPacket *pkt;
    int ret;

    if (!m || !m->stream || !m->header_written || !data || len <= 0)
        return -1;

    pkt = av_packet_alloc();
    if (!pkt) return -1;

    /* copy data — av_interleaved_write_frame takes ownership */
    if (av_new_packet(pkt, len) < 0) {
        av_packet_free(&pkt);
        return -1;
    }
    memcpy(pkt->data, data, len);
    pkt->stream_index = m->stream->index;

    /* convert ms → timebase units:
     *   pts_ms       = ms
     *   fps_num/den  = framerate
     *   pts in stream timebase = pts_ms / 1000 * fps_num / fps_den
     *                          = pts_ms * fps_num / (fps_den * 1000)
     *
     * More simply, we use the AVRational rescale:
     *   pts_ms / 1000 seconds → timebase = {fps_den, fps_num}
     *   pts_stream = pts_ms * fps_num / (fps_den * 1000)
     */
    pkt->pts = av_rescale_q(pts_ms,
                            (AVRational){1, 1000},
                            m->stream->time_base);
    pkt->dts = pkt->pts;

    if (keyframe)
        pkt->flags |= AV_PKT_FLAG_KEY;

    /* ensure monotonically increasing dts; keep pts in sync */
    if (pkt->dts <= m->last_dts) {
        int64_t delta = m->last_dts + 1 - pkt->dts;
        pkt->dts = m->last_dts + 1;
        pkt->pts += delta;
    }
    m->last_dts = pkt->dts;

    ret = av_interleaved_write_frame(m->ctx, pkt);
    av_packet_free(&pkt);

    return (ret < 0) ? -1 : 0;
}

/* ── destroy ────────────────────────────────────────────────────────── */

void
ml_mux_destroy(ml_mux_t *m)
{
    if (!m) return;

    if (m->header_written)
        av_write_trailer(m->ctx);

    if (m->ctx && !(m->ctx->flags & AVFMT_NOFILE))
        avio_closep(&m->ctx->pb);

    avformat_free_context(m->ctx);
    free(m);
}
