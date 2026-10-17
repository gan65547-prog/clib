/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_mux.c — FFmpeg libavformat muxer for H.264/H.265 → MP4/FLV/MKV.
 */

#include "ml_mux.h"
#include "ml_pool.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include <stdlib.h>
#include <string.h>

struct ml_mux_s {
    AVFormatContext *ctx;
    AVStream        *stream;
    ml_pool_t       *pool;
    int              fps_num;
    int              fps_den;
    int              header_written;
    int64_t          last_dts;
};

static void *mux_alloc(ml_pool_t *pool, size_t size)
{
    if (pool)
        return pool->f_alloc(pool, size);
    return malloc(size);
}

static void mux_free(ml_pool_t *pool, void *p)
{
    if (!p) return;
    if (pool)
        pool->f_free(pool, p);
    else
        free(p);
}

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
ml_mux_create(const char *filename, ml_mux_format_e fmt, ml_pool_t *pool)
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

    m = (ml_mux_t *)mux_alloc(pool, sizeof(*m));
    if (!m) return NULL;
    memset(m, 0, sizeof(*m));
    m->pool = pool;

    ret = avformat_alloc_output_context2(&m->ctx, ofmt, NULL, filename);
    if (ret < 0 || !m->ctx) {
        mux_free(pool, m);
        return NULL;
    }

    return m;
}

/* ── configuration ──────────────────────────────────────────────────── */

int
ml_mux_set_video(ml_mux_t *m, ml_mux_codec_e codec,
                 int width, int height,
                 int fps_num, int fps_den,
                 const uint8_t *extradata, size_t extradata_len)
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

    m->stream->time_base = (AVRational){ fps_den, fps_num };

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
ml_mux_write_frame(ml_mux_t *m, const uint8_t *data, size_t len,
                   int64_t pts_ms, int keyframe)
{
    AVPacket *pkt;
    int ret;

    if (!m || !m->stream || !m->header_written || !data || len == 0)
        return -1;

    pkt = av_packet_alloc();
    if (!pkt) return -1;

    if (av_new_packet(pkt, len) < 0) {
        av_packet_free(&pkt);
        return -1;
    }
    memcpy(pkt->data, data, len);
    pkt->stream_index = m->stream->index;

    pkt->pts = av_rescale_q(pts_ms,
                            (AVRational){1, 1000},
                            m->stream->time_base);
    pkt->dts = pkt->pts;

    if (keyframe)
        pkt->flags |= AV_PKT_FLAG_KEY;

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
    ml_pool_t *pool;

    if (!m) return;

    if (m->header_written)
        av_write_trailer(m->ctx);

    if (m->ctx && !(m->ctx->flags & AVFMT_NOFILE))
        avio_closep(&m->ctx->pb);

    avformat_free_context(m->ctx);

    pool = m->pool;
    mux_free(pool, m);
}
